#pragma once

#include "engine/Systems.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

/// Controller가 possess할 수 있는 Actor의 최소 기반이다.
class Pawn : public Actor {
public:
    Pawn(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;

    virtual void addMovementInput(const glm::vec3& direction, float scale);
};

class Character : public Pawn {
public:
    /// 큐브 표시와 Box 충돌을 조합하고 입력을 cm/s 이동으로 바꾼다.
    Character(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void onConstruction() override;
    void tick(float deltaTime) override;
    void addMovementInput(const glm::vec3& direction, float scale) override;

    [[nodiscard]] float moveSpeed() const;
    void setMoveSpeed(float speed);
    [[nodiscard]] const MovementResult& lastMovement() const;

private:
    BoxComponent* collision_{nullptr};
    glm::vec3 pendingMovement_{0.0F};
    MovementResult lastMovement_;
    float moveSpeed_{400.0F};
    float stepHeight_{35.0F};
};

class Controller : public Actor {
public:
    /// Pawn을 소유하지 않고 제어 대상 포인터와 저장용 GUID만 기억한다.
    Controller(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void possess(Pawn* pawn);
    [[nodiscard]] Pawn* pawn() const;
    [[nodiscard]] Guid pawnGuid() const;
    void setPawnGuid(Guid guid);
    void onActorDestroyed(Actor& actor) override;

private:
    Pawn* pawn_{nullptr};
};

class PlayerController : public Controller {
public:
    /// InputSystem의 MoveForward/MoveRight 축을 possess한 Pawn에 전달한다.
    PlayerController(std::string name, World* world);

    [[nodiscard]] std::string_view typeName() const override;
    void tick(float deltaTime) override;
};

class PlayerStart : public Actor {
public:
    PlayerStart(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;
    void onConstruction() override;
};

class GameMode : public Actor {
public:
    GameMode(std::string name, World* world);
    [[nodiscard]] std::string_view typeName() const override;
};

/// Actor의 생명력을 저장하고 피해와 회복 규칙을 작게 캡슐화한다.
class HealthComponent : public ActorComponent {
public:
    HealthComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] float maxHealth() const;
    void setMaxHealth(float value);
    [[nodiscard]] float currentHealth() const;
    void setCurrentHealth(float value);
    [[nodiscard]] bool dead() const;
    void applyDamage(float amount);
    void heal(float amount);

private:
    float maxHealth_{100.0F};
    float currentHealth_{100.0F};
};

/// 매 tick 이동 구간에 raycast를 쏘고, 맞은 Actor의 HealthComponent에 피해를 준다.
class ProjectileComponent : public ActorComponent {
public:
    ProjectileComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    void tickComponent(float deltaTime) override;

    [[nodiscard]] const glm::vec3& velocity() const;
    void setVelocity(const glm::vec3& velocity);
    [[nodiscard]] float damage() const;
    void setDamage(float damage);
    [[nodiscard]] float lifetime() const;
    void setLifetime(float seconds);
    void setInstigator(Actor* actor);

private:
    glm::vec3 velocity_{0.0F};
    float damage_{20.0F};
    float lifetime_{3.0F};
    float age_{0.0F};
    Guid instigator_;
};

/// 소유 Actor 기준으로 간단한 투사체 Actor를 생성하는 전투 시작점이다.
class CombatComponent : public ActorComponent {
public:
    CombatComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] float projectileSpeed() const;
    void setProjectileSpeed(float speed);
    [[nodiscard]] float projectileDamage() const;
    void setProjectileDamage(float damage);
    [[nodiscard]] float projectileLifetime() const;
    void setProjectileLifetime(float seconds);
    Actor* fireProjectile(const glm::vec3& direction);

private:
    float projectileSpeed_{1200.0F};
    float projectileDamage_{20.0F};
    float projectileLifetime_{3.0F};
};

/// BoxComponent를 간단한 dynamic rigid body처럼 움직인다. 내부 adapter는 나중에 Jolt로 교체한다.
class RigidBodyComponent : public ActorComponent {
public:
    RigidBodyComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    void tickComponent(float deltaTime) override;

    [[nodiscard]] const glm::vec3& velocity() const;
    void setVelocity(const glm::vec3& velocity);
    [[nodiscard]] float mass() const;
    void setMass(float mass);
    [[nodiscard]] bool dynamic() const;
    void setDynamic(bool dynamic);
    [[nodiscard]] bool gravityEnabled() const;
    void setGravityEnabled(bool enabled);
    [[nodiscard]] bool grounded() const;
    [[nodiscard]] const MovementResult& lastMovement() const;

private:
    RigidBodyState state_;
    MovementResult lastMovement_;
    JoltRigidBodyAdapter adapter_;
};

/// SceneComponent를 bone처럼 보고 JSON keyframe clip으로 움직이는 교육용 skeletal animation이다.
class SkeletalAnimationComponent : public ActorComponent {
public:
    SkeletalAnimationComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    void tickComponent(float deltaTime) override;

    [[nodiscard]] const std::string& clipJson() const;
    void setClipJson(std::string clip);
    [[nodiscard]] bool playing() const;
    void setPlaying(bool playing);
    [[nodiscard]] float playbackTime() const;
    void setPlaybackTime(float seconds);
    [[nodiscard]] float length() const;
    [[nodiscard]] int appliedPoseCount() const;
    [[nodiscard]] int applyPose(float seconds);

private:
    struct Keyframe {
        float time{0.0F};
        Transform transform;
    };

    struct Track {
        std::string bone;
        std::vector<Keyframe> keys;
    };

    [[nodiscard]] SceneComponent* findBone(std::string_view boneName) const;
    [[nodiscard]] Transform sampleTrack(const Track& track, float seconds) const;
    void rebuildClip();

    std::string clipJson_;
    std::vector<Track> tracks_;
    float length_{0.0F};
    float playbackTime_{0.0F};
    int appliedPoseCount_{0};
    bool playing_{true};
    bool loop_{true};
};

/// 작은 Blueprint 학습용 컴포넌트다. JSON 이벤트 그래프를 읽어 reflection property를 조작한다.
class BlueprintComponent : public ActorComponent {
public:
    BlueprintComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    void beginPlay() override;
    void tickComponent(float deltaTime) override;

    [[nodiscard]] const std::string& graphJson() const;
    void setGraphJson(std::string graph);
    [[nodiscard]] int executionCount() const;
    [[nodiscard]] int executeEvent(std::string_view eventName, float deltaTime = 0.0F);

private:
    struct Action {
        std::string eventName;
        std::string action;
        std::string target;
        std::string property;
        PropertyValue value;
        bool scaleByDelta{false};
    };

    [[nodiscard]] Object* resolveTarget(const Action& action) const;
    [[nodiscard]] bool applyAction(const Action& action, float deltaTime);
    void rebuildActions();

    std::string graphJson_;
    std::vector<Action> actions_;
    int executionCount_{0};
};

class GameInstance : public Object {
public:
    /// World가 바뀌어도 EngineRuntime과 함께 유지되는 게임 세션 객체다.
    GameInstance();
    [[nodiscard]] std::string_view typeName() const override;
};

enum class EditorMode {
    Edit,
    Simulate,
    PlayInEditor,
};

class EngineRuntime {
public:
    /// Edit World와 임시 PIE World의 소유권, 실행 모드, 공용 시스템을 관리한다.
    EngineRuntime();

    void setEditWorld(std::unique_ptr<World> world);
    [[nodiscard]] World& editWorld();
    [[nodiscard]] const World& editWorld() const;
    [[nodiscard]] World& activeWorld();
    [[nodiscard]] const World& activeWorld() const;

    bool startSimulate();
    bool startPlayInEditor();
    void stop();
    void tick(float deltaTime);

    [[nodiscard]] EditorMode mode() const;
    [[nodiscard]] InputSystem& input();
    [[nodiscard]] OutputLog& log();
    [[nodiscard]] AssetRegistry& assets();
    [[nodiscard]] TransactionStack& transactions();

private:
    void connectInput(World& world);

    GameInstance gameInstance_;
    std::unique_ptr<World> editWorld_;
    std::unique_ptr<World> playWorld_;
    InputSystem input_;
    OutputLog log_;
    AssetRegistry assets_;
    TransactionStack transactions_;
    EditorMode mode_{EditorMode::Edit};
};

} // namespace engine
