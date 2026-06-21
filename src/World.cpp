#include "engine/World.hpp"

#include "engine/Systems.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace {

bool shouldTick(
    engine::TickSettings settings,
    engine::TickGroup group,
    float deltaTime,
    float& accumulator
) {
    if (!settings.enabled || settings.group != group) {
        return false;
    }
    if (settings.interval <= 0.0F) {
        return true;
    }

    accumulator += deltaTime;
    if (accumulator < settings.interval) {
        return false;
    }
    accumulator = 0.0F;
    return true;
}

} // namespace

namespace engine {

ActorComponent::ActorComponent(std::string name, Actor* owner)
    : Object(std::move(name), owner),
      owner_(owner) {}

std::string_view ActorComponent::typeName() const {
    return "ActorComponent";
}

Actor* ActorComponent::owner() const {
    return owner_;
}

bool ActorComponent::registered() const {
    return registered_;
}

TickSettings& ActorComponent::tickSettings() {
    return tickSettings_;
}

const TickSettings& ActorComponent::tickSettings() const {
    return tickSettings_;
}

void ActorComponent::onRegister() {}
void ActorComponent::beginPlay() {}
void ActorComponent::tickComponent(float) {}
void ActorComponent::endPlay() {}
void ActorComponent::onUnregister() {}

void ActorComponent::registerComponent() {
    if (registered_) {
        return;
    }
    registered_ = true;
    onRegister();
}

void ActorComponent::unregisterComponent() {
    if (!registered_) {
        return;
    }
    if (beganPlay_) {
        endPlay();
        beganPlay_ = false;
    }
    onUnregister();
    registered_ = false;
}

void ActorComponent::tickForGroup(TickGroup group, float deltaTime) {
    if (registered_ &&
        shouldTick(tickSettings_, group, deltaTime, tickAccumulator_)) {
        tickComponent(deltaTime);
    }
}

SceneComponent::SceneComponent(std::string name, Actor* owner)
    : ActorComponent(std::move(name), owner) {}

SceneComponent::~SceneComponent() {
    detach();
    for (SceneComponent* child : children_) {
        child->parent_ = nullptr;
        child->markTransformDirty();
    }
}

std::string_view SceneComponent::typeName() const {
    return "SceneComponent";
}

const Transform& SceneComponent::relativeTransform() const {
    return relativeTransform_;
}

void SceneComponent::setRelativeTransform(const Transform& transform) {
    relativeTransform_ = transform;
    markTransformDirty();
}

void SceneComponent::setRelativeLocation(const glm::vec3& location) {
    relativeTransform_.location = location;
    markTransformDirty();
}

void SceneComponent::setRelativeRotation(const glm::vec3& rotationDegrees) {
    relativeTransform_.rotationDegrees = rotationDegrees;
    markTransformDirty();
}

void SceneComponent::setRelativeScale(const glm::vec3& scale) {
    relativeTransform_.scale = scale;
    markTransformDirty();
}

Transform SceneComponent::worldTransform() const {
    return parent_ == nullptr
        ? relativeTransform_
        : Transform::combine(parent_->worldTransform(), relativeTransform_);
}

glm::mat4 SceneComponent::worldMatrix() const {
    return parent_ == nullptr
        ? relativeTransform_.matrix()
        : parent_->worldMatrix() * relativeTransform_.matrix();
}

SceneComponent* SceneComponent::parent() const {
    return parent_;
}

const std::vector<SceneComponent*>& SceneComponent::children() const {
    return children_;
}

bool SceneComponent::attachTo(SceneComponent* newParent) {
    // 자기 자신, 자기 자손, 다른 Actor의 Component를 부모로 삼지 못하게 한다.
    // 이 제한 덕분에 계층 순환 없이 Actor 단위 소유권을 유지할 수 있다.
    if (newParent == this ||
        (newParent != nullptr && newParent->isDescendantOf(this)) ||
        (newParent != nullptr && newParent->owner() != owner())) {
        return false;
    }

    detach();
    parent_ = newParent;
    if (parent_ != nullptr) {
        parent_->children_.push_back(this);
    }
    markTransformDirty();
    return true;
}

void SceneComponent::detach() {
    if (parent_ == nullptr) {
        return;
    }
    std::erase(parent_->children_, this);
    parent_ = nullptr;
    markTransformDirty();
}

void SceneComponent::onTransformChanged() {}

void SceneComponent::markTransformDirty() {
    onTransformChanged();
    for (SceneComponent* child : children_) {
        child->markTransformDirty();
    }
}

bool SceneComponent::isDescendantOf(
    const SceneComponent* candidate
) const {
    for (const SceneComponent* current = parent_; current != nullptr;
         current = current->parent_) {
        if (current == candidate) {
            return true;
        }
    }
    return false;
}

Actor::Actor(std::string name, World* world)
    : Object(std::move(name), world),
      world_(world) {}

Actor::~Actor() {
    unregisterComponents();
}

std::string_view Actor::typeName() const {
    return "Actor";
}

World* Actor::world() const {
    return world_;
}

ActorComponent* Actor::addComponentByType(
    const TypeDescriptor& type,
    std::string name
) {
    if (!type.factory) {
        return nullptr;
    }
    std::unique_ptr<Object> created = type.factory(this, std::move(name));
    auto* component = dynamic_cast<ActorComponent*>(created.get());
    if (component == nullptr) {
        return nullptr;
    }

    created.release();
    addOwnedComponent(std::unique_ptr<ActorComponent>{component});
    return component;
}

const std::vector<std::unique_ptr<ActorComponent>>& Actor::components() const {
    return components_;
}

void Actor::setRootComponent(SceneComponent* component) {
    if (component != nullptr && component->owner() != this) {
        throw std::invalid_argument("Root component must belong to its Actor.");
    }
    rootComponent_ = component;
    if (rootComponent_ != nullptr) {
        rootComponent_->detach();
    }
}

SceneComponent* Actor::rootComponent() const {
    return rootComponent_;
}

Transform Actor::actorTransform() const {
    return rootComponent_ == nullptr ? Transform{} : rootComponent_->worldTransform();
}

TickSettings& Actor::tickSettings() {
    return tickSettings_;
}

const TickSettings& Actor::tickSettings() const {
    return tickSettings_;
}

bool Actor::pendingDestroy() const {
    return pendingDestroy_;
}

void Actor::onConstruction() {}
void Actor::beginPlay() {}
void Actor::tick(float) {}
void Actor::endPlay() {}
void Actor::onActorDestroyed(Actor&) {}

void Actor::addOwnedComponent(std::unique_ptr<ActorComponent> component) {
    ActorComponent* pointer = component.get();
    components_.push_back(std::move(component));

    if (componentsRegistered_) {
        pointer->registerComponent();
        if (beganPlay_) {
            pointer->beginPlay();
            pointer->beganPlay_ = true;
        }
    }
}

void Actor::registerComponents() {
    if (componentsRegistered_) {
        return;
    }
    for (const auto& component : components_) {
        component->registerComponent();
    }
    componentsRegistered_ = true;
}

void Actor::unregisterComponents() {
    for (auto iterator = components_.rbegin(); iterator != components_.rend();
         ++iterator) {
        (*iterator)->unregisterComponent();
    }
    componentsRegistered_ = false;
}

void Actor::beginPlayInternal() {
    if (beganPlay_) {
        return;
    }
    beginPlay();
    for (const auto& component : components_) {
        component->beginPlay();
        component->beganPlay_ = true;
    }
    beganPlay_ = true;
}

void Actor::endPlayInternal() {
    if (!beganPlay_) {
        return;
    }
    for (auto iterator = components_.rbegin(); iterator != components_.rend();
         ++iterator) {
        if ((*iterator)->beganPlay_) {
            (*iterator)->endPlay();
            (*iterator)->beganPlay_ = false;
        }
    }
    endPlay();
    beganPlay_ = false;
}

void Actor::tickForGroup(TickGroup group, float deltaTime) {
    if (pendingDestroy_) {
        return;
    }
    if (shouldTick(tickSettings_, group, deltaTime, tickAccumulator_)) {
        tick(deltaTime);
    }
    if (pendingDestroy_) {
        return;
    }
    for (const auto& component : components_) {
        component->tickForGroup(group, deltaTime);
    }
}

void Actor::clearComponentsForLoad() {
    unregisterComponents();
    components_.clear();
    rootComponent_ = nullptr;
}

World::World(std::string name)
    : Object(std::move(name), nullptr),
      collisionWorld_(std::make_unique<CollisionWorld>()),
      renderScene_(std::make_unique<RenderScene>()) {}

World::~World() {
    endPlay();
}

std::string_view World::typeName() const {
    return "World";
}

Actor* World::spawnActorByType(
    const TypeDescriptor& type,
    std::string name,
    bool runConstruction
) {
    if (!type.factory) {
        return nullptr;
    }
    std::unique_ptr<Object> created = type.factory(this, std::move(name));
    auto* actor = dynamic_cast<Actor*>(created.get());
    if (actor == nullptr) {
        return nullptr;
    }

    created.release();
    return adoptActor(std::unique_ptr<Actor>{actor}, runConstruction);
}

void World::destroyActor(Actor& actor) {
    if (actor.world() == this) {
        for (const auto& observer : actors_) {
            if (observer.get() != &actor) {
                observer->onActorDestroyed(actor);
            }
        }
        actor.pendingDestroy_ = true;
        if (!ticking_) {
            flushDeferredChanges();
        }
    }
}

Actor* World::findActor(Guid guid) const {
    for (const auto& actor : actors_) {
        if (actor->guid() == guid) {
            return actor.get();
        }
    }
    for (const auto& [actor, runConstruction] : pendingSpawns_) {
        (void)runConstruction;
        if (actor->guid() == guid) {
            return actor.get();
        }
    }
    return nullptr;
}

Object* World::findObject(Guid guid) const {
    if (guid == this->guid()) {
        return const_cast<World*>(this);
    }
    for (const auto& actor : actors_) {
        if (actor->guid() == guid) {
            return actor.get();
        }
        for (const auto& component : actor->components()) {
            if (component->guid() == guid) {
                return component.get();
            }
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<Actor>>& World::actors() const {
    return actors_;
}

void World::beginPlay() {
    if (beganPlay_) {
        return;
    }
    beganPlay_ = true;
    for (const auto& actor : actors_) {
        actor->beginPlayInternal();
    }
}

void World::tick(float deltaTime) {
    static constexpr std::array groups{
        TickGroup::PrePhysics,
        TickGroup::Physics,
        TickGroup::PostPhysics,
        TickGroup::PostUpdate,
    };

    // 그룹 실행 중 컨테이너를 변경하지 않는다. 이때 생긴 spawn/destroy 요청은
    // flushDeferredChanges에서 한 번에 반영한다.
    ticking_ = true;
    for (TickGroup group : groups) {
        for (const auto& actor : actors_) {
            actor->tickForGroup(group, deltaTime);
        }
        if (group == TickGroup::Physics) {
            collisionWorld_->updateOverlaps(*this);
        }
    }
    ticking_ = false;
    flushDeferredChanges();
}

void World::endPlay() {
    if (!beganPlay_) {
        return;
    }
    for (auto iterator = actors_.rbegin(); iterator != actors_.rend(); ++iterator) {
        (*iterator)->endPlayInternal();
    }
    beganPlay_ = false;
}

bool World::hasBegunPlay() const {
    return beganPlay_;
}

CollisionWorld& World::collision() {
    return *collisionWorld_;
}

const CollisionWorld& World::collision() const {
    return *collisionWorld_;
}

RenderScene& World::renderScene() {
    return *renderScene_;
}

const RenderScene& World::renderScene() const {
    return *renderScene_;
}

void World::setInputSystem(InputSystem* inputSystem) {
    inputSystem_ = inputSystem;
}

InputSystem* World::inputSystem() const {
    return inputSystem_;
}

Actor* World::adoptActor(
    std::unique_ptr<Actor> actor,
    bool runConstruction
) {
    Actor* pointer = actor.get();
    // Tick 중 push_back은 현재 순회 중인 vector의 반복자를 무효화할 수 있다.
    if (ticking_) {
        pendingSpawns_.emplace_back(std::move(actor), runConstruction);
    } else {
        actors_.push_back(std::move(actor));
        initializeActor(*pointer, runConstruction);
    }
    return pointer;
}

void World::initializeActor(Actor& actor, bool runConstruction) {
    if (runConstruction) {
        actor.onConstruction();
    }
    actor.registerComponents();
    if (beganPlay_) {
        actor.beginPlayInternal();
    }
}

void World::flushDeferredChanges() {
    // 생성은 먼저 초기화해 같은 프레임 끝부터 조회 가능하게 하고,
    // 삭제는 EndPlay와 component 해제를 보장한 뒤 vector에서 제거한다.
    for (auto& [actor, runConstruction] : pendingSpawns_) {
        Actor* pointer = actor.get();
        actors_.push_back(std::move(actor));
        initializeActor(*pointer, runConstruction);
    }
    pendingSpawns_.clear();

    std::erase_if(actors_, [](const std::unique_ptr<Actor>& actor) {
        if (!actor->pendingDestroy_) {
            return false;
        }
        actor->endPlayInternal();
        actor->unregisterComponents();
        return true;
    });
}

void World::clearForLoad() {
    endPlay();
    pendingSpawns_.clear();
    actors_.clear();
}

} // namespace engine
