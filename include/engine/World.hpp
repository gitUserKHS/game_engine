#pragma once

#include "engine/Core.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine {

class Actor;
class ActorComponent;
class SceneComponent;
class CollisionWorld;
class InputSystem;
class RenderScene;
class WorldSerializer;

/// Actor가 소유하는 기능 단위다. 등록 후 BeginPlay를 거쳐 선택적으로 Tick한다.
class ActorComponent : public Object {
public:
    ActorComponent(std::string name, Actor* owner);
    ~ActorComponent() override = default;

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] Actor* owner() const;
    [[nodiscard]] bool registered() const;

    TickSettings& tickSettings();
    [[nodiscard]] const TickSettings& tickSettings() const;

    virtual void onRegister();
    virtual void beginPlay();
    virtual void tickComponent(float deltaTime);
    virtual void endPlay();
    virtual void onUnregister();

private:
    friend class Actor;
    friend class World;

    void registerComponent();
    void unregisterComponent();
    void tickForGroup(TickGroup group, float deltaTime);

    Actor* owner_{nullptr};
    TickSettings tickSettings_;
    float tickAccumulator_{0.0F};
    bool registered_{false};
    bool beganPlay_{false};
};

class SceneComponent : public ActorComponent {
public:
    /// Transform은 부모 기준 상대값으로 저장하고 요청할 때 World 값으로 합성한다.
    SceneComponent(std::string name, Actor* owner);
    ~SceneComponent() override;

    [[nodiscard]] std::string_view typeName() const override;

    [[nodiscard]] const Transform& relativeTransform() const;
    void setRelativeTransform(const Transform& transform);
    void setRelativeLocation(const glm::vec3& location);
    void setRelativeRotation(const glm::vec3& rotationDegrees);
    void setRelativeScale(const glm::vec3& scale);

    [[nodiscard]] Transform worldTransform() const;
    [[nodiscard]] glm::mat4 worldMatrix() const;

    [[nodiscard]] SceneComponent* parent() const;
    [[nodiscard]] const std::vector<SceneComponent*>& children() const;
    /// 순환 계층이 되면 false를 반환하며 기존 attachment를 유지한다.
    bool attachTo(SceneComponent* newParent);
    void detach();

protected:
    virtual void onTransformChanged();
    void markTransformDirty();

private:
    [[nodiscard]] bool isDescendantOf(const SceneComponent* candidate) const;

    Transform relativeTransform_;
    SceneComponent* parent_{nullptr};
    std::vector<SceneComponent*> children_;
};

class Actor : public Object {
public:
    /// World가 수명을 소유한다. 공간상 위치는 rootComponent가 대표한다.
    Actor(std::string name, class World* world);
    ~Actor() override;

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] class World* world() const;

    template<typename Component, typename... Arguments>
    Component& addComponent(std::string name, Arguments&&... arguments) {
        static_assert(std::is_base_of_v<ActorComponent, Component>);
        auto component = std::make_unique<Component>(
            std::move(name),
            this,
            std::forward<Arguments>(arguments)...
        );
        Component* pointer = component.get();
        addOwnedComponent(std::move(component));
        return *pointer;
    }

    ActorComponent* addComponentByType(
        const TypeDescriptor& type,
        std::string name
    );

    template<typename Component>
    [[nodiscard]] Component* findComponent() const {
        for (const auto& component : components_) {
            if (auto* result = dynamic_cast<Component*>(component.get())) {
                return result;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<ActorComponent>>&
    components() const;

    void setRootComponent(SceneComponent* component);
    [[nodiscard]] SceneComponent* rootComponent() const;
    [[nodiscard]] Transform actorTransform() const;

    TickSettings& tickSettings();
    [[nodiscard]] const TickSettings& tickSettings() const;
    [[nodiscard]] bool pendingDestroy() const;

    virtual void onConstruction();
    virtual void beginPlay();
    virtual void tick(float deltaTime);
    virtual void endPlay();

private:
    friend class World;
    friend class WorldSerializer;

    void addOwnedComponent(std::unique_ptr<ActorComponent> component);
    void registerComponents();
    void unregisterComponents();
    void beginPlayInternal();
    void endPlayInternal();
    void tickForGroup(TickGroup group, float deltaTime);
    void clearComponentsForLoad();

    class World* world_{nullptr};
    std::vector<std::unique_ptr<ActorComponent>> components_;
    SceneComponent* rootComponent_{nullptr};
    TickSettings tickSettings_;
    float tickAccumulator_{0.0F};
    bool componentsRegistered_{false};
    bool beganPlay_{false};
    bool pendingDestroy_{false};
};

class World : public Object {
public:
    /// Actor, 충돌, 렌더 장면의 수명을 묶고 고정 update의 생명주기를 실행한다.
    explicit World(std::string name = "World");
    ~World() override;

    [[nodiscard]] std::string_view typeName() const override;

    template<typename ActorType, typename... Arguments>
    ActorType& spawnActor(std::string name, Arguments&&... arguments) {
        static_assert(std::is_base_of_v<Actor, ActorType>);
        auto actor = std::make_unique<ActorType>(
            std::move(name),
            this,
            std::forward<Arguments>(arguments)...
        );
        ActorType* pointer = actor.get();
        adoptActor(std::move(actor), true);
        return *pointer;
    }

    Actor* spawnActorByType(
        const TypeDescriptor& type,
        std::string name,
        bool runConstruction = true
    );
    /// Tick 중 호출해도 안전하도록 삭제는 현재 update 끝까지 지연될 수 있다.
    void destroyActor(Actor& actor);

    [[nodiscard]] Actor* findActor(Guid guid) const;
    [[nodiscard]] Object* findObject(Guid guid) const;
    [[nodiscard]] const std::vector<std::unique_ptr<Actor>>& actors() const;

    template<typename Component>
    [[nodiscard]] std::vector<Component*> componentsOfType() const {
        std::vector<Component*> result;
        for (const auto& actor : actors_) {
            for (const auto& component : actor->components()) {
                if (auto* typed = dynamic_cast<Component*>(component.get())) {
                    result.push_back(typed);
                }
            }
        }
        return result;
    }

    void beginPlay();
    void tick(float deltaTime);
    void endPlay();
    [[nodiscard]] bool hasBegunPlay() const;

    CollisionWorld& collision();
    [[nodiscard]] const CollisionWorld& collision() const;
    RenderScene& renderScene();
    [[nodiscard]] const RenderScene& renderScene() const;

    void setInputSystem(InputSystem* inputSystem);
    [[nodiscard]] InputSystem* inputSystem() const;

private:
    friend class WorldSerializer;

    Actor* adoptActor(std::unique_ptr<Actor> actor, bool runConstruction);
    void initializeActor(Actor& actor, bool runConstruction);
    void flushDeferredChanges();
    void clearForLoad();

    std::vector<std::unique_ptr<Actor>> actors_;
    std::vector<std::pair<std::unique_ptr<Actor>, bool>> pendingSpawns_;
    std::unique_ptr<CollisionWorld> collisionWorld_;
    std::unique_ptr<RenderScene> renderScene_;
    InputSystem* inputSystem_{nullptr};
    bool beganPlay_{false};
    bool ticking_{false};
};

} // namespace engine
