#include "engine/Gameplay.hpp"

#include <nlohmann/json.hpp>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

using Json = nlohmann::json;

engine::PropertyValue blueprintValueFromJson(
    engine::PropertyType type,
    const Json& value
) {
    using engine::Guid;
    using engine::PropertyType;

    switch (type) {
    case PropertyType::Boolean:
        return value.get<bool>();
    case PropertyType::Integer:
        return value.get<int>();
    case PropertyType::Float:
        return value.get<float>();
    case PropertyType::String:
        return value.get<std::string>();
    case PropertyType::Vector3:
        return glm::vec3{
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
        };
    case PropertyType::Guid:
        return Guid::parse(value.get<std::string>()).value_or(Guid{});
    }
    return {};
}

const engine::PropertyDescriptor* findProperty(
    const engine::Object& object,
    std::string_view name
) {
    const engine::TypeDescriptor* type = object.typeDescriptor();
    if (type == nullptr) {
        return nullptr;
    }
    for (const engine::PropertyDescriptor* property : type->allProperties()) {
        if (property->name == name) {
            return property;
        }
    }
    return nullptr;
}

glm::vec3 vectorFromJsonOr(
    const Json& json,
    std::string_view key,
    const glm::vec3& fallback
) {
    const auto found = json.find(std::string{key});
    if (found == json.end() || !found->is_array() || found->size() != 3) {
        return fallback;
    }
    return {
        found->at(0).get<float>(),
        found->at(1).get<float>(),
        found->at(2).get<float>(),
    };
}

engine::Transform transformFromKeyJson(const Json& json) {
    engine::Transform transform;
    transform.location = vectorFromJsonOr(json, "location", transform.location);
    transform.rotationDegrees =
        vectorFromJsonOr(json, "rotation", transform.rotationDegrees);
    transform.scale = vectorFromJsonOr(json, "scale", transform.scale);
    return transform;
}

} // namespace

namespace engine {

Pawn::Pawn(std::string name, World* world)
    : Actor(std::move(name), world) {}

std::string_view Pawn::typeName() const {
    return "Pawn";
}

void Pawn::addMovementInput(const glm::vec3&, float) {}

Character::Character(std::string name, World* world)
    : Pawn(std::move(name), world) {
    tickSettings().enabled = true;
    tickSettings().group = TickGroup::Physics;
}

std::string_view Character::typeName() const {
    return "Character";
}

void Character::onConstruction() {
    collision_ = &addComponent<BoxComponent>("Collision");
    collision_->setExtent({45.0F, 45.0F, 45.0F});
    collision_->setObjectChannel(CollisionChannel::Pawn);
    collision_->setDrawDebug(true);
    collision_->setRelativeLocation({0.0F, 0.0F, 45.0F});
    setRootComponent(collision_);

    auto& bodyBone = addComponent<SceneComponent>("BodyBone");
    bodyBone.attachTo(collision_);

    auto& mesh = addComponent<StaticMeshComponent>("PlayerMesh");
    mesh.attachTo(&bodyBone);
    Transform meshTransform;
    meshTransform.scale = {90.0F, 90.0F, 90.0F};
    mesh.setRelativeTransform(meshTransform);
    MaterialInstance material;
    material.baseColor = {0.25F, 0.65F, 0.95F};
    mesh.setMaterial(material);

    auto& springArm = addComponent<SpringArmComponent>("CameraBoom");
    springArm.attachTo(collision_);
    springArm.setTargetArmLength(1000.0F);

    auto& camera = addComponent<CameraComponent>("Camera");
    camera.attachTo(&springArm);
    Transform cameraTransform;
    cameraTransform.location = {-850.0F, 0.0F, 950.0F};
    cameraTransform.rotationDegrees = {0.0F, 42.0F, 0.0F};
    camera.setRelativeTransform(cameraTransform);

    auto& animation = addComponent<SkeletalAnimationComponent>("Animation");
    animation.setClipJson(R"({
  "length": 1.0,
  "loop": true,
  "tracks": [
    {
      "bone": "BodyBone",
      "keys": [
        {"time": 0.0, "rotation": [0.0, 0.0, -3.0]},
        {"time": 0.5, "rotation": [0.0, 0.0, 3.0]},
        {"time": 1.0, "rotation": [0.0, 0.0, -3.0]}
      ]
    }
  ]
})");
}

void Character::tick(float deltaTime) {
    if (collision_ == nullptr) {
        collision_ = findComponent<BoxComponent>();
    }
    if (collision_ == nullptr) {
        return;
    }

    // 대각선 입력이 단일 축보다 빠르지 않도록 누적 입력의 길이를 1로 제한한다.
    if (glm::dot(pendingMovement_, pendingMovement_) > 1.0F) {
        pendingMovement_ = glm::normalize(pendingMovement_);
    }
    const glm::vec3 delta = pendingMovement_ * moveSpeed_ * deltaTime;
    lastMovement_ = world()->collision().moveComponentStepped(
        *collision_,
        delta,
        stepHeight_,
        *world()
    );
    pendingMovement_ = {0.0F, 0.0F, 0.0F};
}

void Character::addMovementInput(
    const glm::vec3& direction,
    float scale
) {
    pendingMovement_ += direction * scale;
}

float Character::moveSpeed() const {
    return moveSpeed_;
}

void Character::setMoveSpeed(float speed) {
    moveSpeed_ = std::max(speed, 0.0F);
}

const MovementResult& Character::lastMovement() const {
    return lastMovement_;
}

Controller::Controller(std::string name, World* world)
    : Actor(std::move(name), world) {}

std::string_view Controller::typeName() const {
    return "Controller";
}

void Controller::possess(Pawn* pawn) {
    if (pawn != nullptr && pawn->world() != world()) {
        return;
    }
    pawn_ = pawn;
}

Pawn* Controller::pawn() const {
    return pawn_;
}

Guid Controller::pawnGuid() const {
    return pawn_ == nullptr ? Guid{} : pawn_->guid();
}

void Controller::setPawnGuid(Guid guid) {
    possess(dynamic_cast<Pawn*>(world()->findActor(guid)));
}

void Controller::onActorDestroyed(Actor& actor) {
    if (pawn_ == &actor) {
        possess(nullptr);
    }
}

PlayerController::PlayerController(std::string name, World* world)
    : Controller(std::move(name), world) {
    tickSettings().enabled = true;
    tickSettings().group = TickGroup::PrePhysics;
}

std::string_view PlayerController::typeName() const {
    return "PlayerController";
}

void PlayerController::tick(float) {
    InputSystem* input = world()->inputSystem();
    if (input == nullptr || pawn() == nullptr) {
        return;
    }

    pawn()->addMovementInput(
        {input->axis("MoveForward"), input->axis("MoveRight"), 0.0F},
        1.0F
    );
}

PlayerStart::PlayerStart(std::string name, World* world)
    : Actor(std::move(name), world) {}

std::string_view PlayerStart::typeName() const {
    return "PlayerStart";
}

void PlayerStart::onConstruction() {
    auto& root = addComponent<SceneComponent>("Root");
    setRootComponent(&root);
}

GameMode::GameMode(std::string name, World* world)
    : Actor(std::move(name), world) {}

std::string_view GameMode::typeName() const {
    return "GameMode";
}

HealthComponent::HealthComponent(std::string name, Actor* owner)
    : ActorComponent(std::move(name), owner) {}

std::string_view HealthComponent::typeName() const {
    return "HealthComponent";
}

float HealthComponent::maxHealth() const {
    return maxHealth_;
}

void HealthComponent::setMaxHealth(float value) {
    maxHealth_ = std::max(value, 1.0F);
    currentHealth_ = std::clamp(currentHealth_, 0.0F, maxHealth_);
}

float HealthComponent::currentHealth() const {
    return currentHealth_;
}

void HealthComponent::setCurrentHealth(float value) {
    currentHealth_ = std::clamp(value, 0.0F, maxHealth_);
}

bool HealthComponent::dead() const {
    return currentHealth_ <= 0.0F;
}

void HealthComponent::applyDamage(float amount) {
    if (amount <= 0.0F) {
        return;
    }
    setCurrentHealth(currentHealth_ - amount);
}

void HealthComponent::heal(float amount) {
    if (amount <= 0.0F) {
        return;
    }
    setCurrentHealth(currentHealth_ + amount);
}

ProjectileComponent::ProjectileComponent(std::string name, Actor* owner)
    : ActorComponent(std::move(name), owner) {
    tickSettings().enabled = true;
    tickSettings().group = TickGroup::Physics;
}

std::string_view ProjectileComponent::typeName() const {
    return "ProjectileComponent";
}

void ProjectileComponent::tickComponent(float deltaTime) {
    Actor* projectile = owner();
    if (projectile == nullptr || projectile->world() == nullptr ||
        projectile->rootComponent() == nullptr) {
        return;
    }

    age_ += deltaTime;
    if (age_ >= lifetime_) {
        projectile->world()->destroyActor(*projectile);
        return;
    }

    const glm::vec3 delta = velocity_ * deltaTime;
    const float distance = glm::length(delta);
    if (distance <= 0.0001F) {
        return;
    }

    const glm::vec3 origin = projectile->actorTransform().location;
    const auto hit = projectile->world()->collision().raycast(
        origin,
        delta,
        distance,
        *projectile->world(),
        CollisionChannel::Pawn,
        projectile->findComponent<BoxComponent>()
    );
    if (hit.has_value()) {
        Actor* target = hit->component->owner();
        if (target != nullptr && target != projectile &&
            target->guid() != instigator_) {
            if (auto* health = target->findComponent<HealthComponent>()) {
                health->applyDamage(damage_);
            }
        }
        projectile->world()->destroyActor(*projectile);
        return;
    }

    Transform transform = projectile->rootComponent()->relativeTransform();
    transform.location += delta;
    projectile->rootComponent()->setRelativeTransform(transform);
}

const glm::vec3& ProjectileComponent::velocity() const {
    return velocity_;
}

void ProjectileComponent::setVelocity(const glm::vec3& velocity) {
    velocity_ = velocity;
}

float ProjectileComponent::damage() const {
    return damage_;
}

void ProjectileComponent::setDamage(float damage) {
    damage_ = std::max(damage, 0.0F);
}

float ProjectileComponent::lifetime() const {
    return lifetime_;
}

void ProjectileComponent::setLifetime(float seconds) {
    lifetime_ = std::max(seconds, 0.01F);
}

void ProjectileComponent::setInstigator(Actor* actor) {
    instigator_ = actor == nullptr ? Guid{} : actor->guid();
}

CombatComponent::CombatComponent(std::string name, Actor* owner)
    : ActorComponent(std::move(name), owner) {}

std::string_view CombatComponent::typeName() const {
    return "CombatComponent";
}

float CombatComponent::projectileSpeed() const {
    return projectileSpeed_;
}

void CombatComponent::setProjectileSpeed(float speed) {
    projectileSpeed_ = std::max(speed, 0.0F);
}

float CombatComponent::projectileDamage() const {
    return projectileDamage_;
}

void CombatComponent::setProjectileDamage(float damage) {
    projectileDamage_ = std::max(damage, 0.0F);
}

float CombatComponent::projectileLifetime() const {
    return projectileLifetime_;
}

void CombatComponent::setProjectileLifetime(float seconds) {
    projectileLifetime_ = std::max(seconds, 0.01F);
}

Actor* CombatComponent::fireProjectile(const glm::vec3& direction) {
    Actor* source = owner();
    if (source == nullptr || source->world() == nullptr ||
        glm::dot(direction, direction) <= 0.0001F) {
        return nullptr;
    }

    const glm::vec3 forward = glm::normalize(direction);
    Actor& projectile = source->world()->spawnActor<Actor>("Projectile");
    auto& collision = projectile.addComponent<BoxComponent>("Collision");
    collision.setExtent({8.0F, 8.0F, 8.0F});
    collision.setObjectChannel(CollisionChannel::WorldDynamic);
    collision.setDrawDebug(true);
    collision.setRelativeLocation(source->actorTransform().location + forward * 70.0F);
    projectile.setRootComponent(&collision);

    auto& mesh = projectile.addComponent<StaticMeshComponent>("Mesh");
    mesh.attachTo(&collision);
    Transform meshTransform;
    meshTransform.scale = {16.0F, 16.0F, 16.0F};
    mesh.setRelativeTransform(meshTransform);
    MaterialInstance material;
    material.baseColor = {1.0F, 0.78F, 0.22F};
    mesh.setMaterial(material);

    auto& projectileComponent =
        projectile.addComponent<ProjectileComponent>("Projectile");
    projectileComponent.setVelocity(forward * projectileSpeed_);
    projectileComponent.setDamage(projectileDamage_);
    projectileComponent.setLifetime(projectileLifetime_);
    projectileComponent.setInstigator(source);
    return &projectile;
}

SkeletalAnimationComponent::SkeletalAnimationComponent(
    std::string name,
    Actor* owner
)
    : ActorComponent(std::move(name), owner) {
    tickSettings().enabled = true;
    tickSettings().group = TickGroup::PostUpdate;
}

std::string_view SkeletalAnimationComponent::typeName() const {
    return "SkeletalAnimationComponent";
}

void SkeletalAnimationComponent::tickComponent(float deltaTime) {
    if (!playing_ || tracks_.empty()) {
        return;
    }
    playbackTime_ += std::max(deltaTime, 0.0F);
    (void)applyPose(playbackTime_);
}

const std::string& SkeletalAnimationComponent::clipJson() const {
    return clipJson_;
}

void SkeletalAnimationComponent::setClipJson(std::string clip) {
    clipJson_ = std::move(clip);
    rebuildClip();
}

bool SkeletalAnimationComponent::playing() const {
    return playing_;
}

void SkeletalAnimationComponent::setPlaying(bool playing) {
    playing_ = playing;
}

float SkeletalAnimationComponent::playbackTime() const {
    return playbackTime_;
}

void SkeletalAnimationComponent::setPlaybackTime(float seconds) {
    playbackTime_ = std::max(seconds, 0.0F);
}

float SkeletalAnimationComponent::length() const {
    return length_;
}

int SkeletalAnimationComponent::appliedPoseCount() const {
    return appliedPoseCount_;
}

int SkeletalAnimationComponent::applyPose(float seconds) {
    if (tracks_.empty()) {
        return 0;
    }

    float localTime = std::max(seconds, 0.0F);
    if (length_ > 0.0001F) {
        if (loop_) {
            localTime = std::fmod(localTime, length_);
        } else {
            localTime = std::min(localTime, length_);
        }
    }

    int applied = 0;
    for (const Track& track : tracks_) {
        SceneComponent* bone = findBone(track.bone);
        if (bone == nullptr || track.keys.empty()) {
            continue;
        }
        bone->setRelativeTransform(sampleTrack(track, localTime));
        ++applied;
    }
    appliedPoseCount_ += applied;
    return applied;
}

SceneComponent* SkeletalAnimationComponent::findBone(
    std::string_view boneName
) const {
    Actor* actor = owner();
    if (actor == nullptr) {
        return nullptr;
    }
    for (const auto& component : actor->components()) {
        auto* scene = dynamic_cast<SceneComponent*>(component.get());
        if (scene != nullptr && scene->name() == boneName) {
            return scene;
        }
    }
    return nullptr;
}

Transform SkeletalAnimationComponent::sampleTrack(
    const Track& track,
    float seconds
) const {
    if (track.keys.size() == 1) {
        return track.keys.front().transform;
    }
    if (seconds <= track.keys.front().time) {
        return track.keys.front().transform;
    }

    for (std::size_t index = 1; index < track.keys.size(); ++index) {
        const Keyframe& previous = track.keys[index - 1];
        const Keyframe& next = track.keys[index];
        if (seconds > next.time) {
            continue;
        }
        const float duration = std::max(next.time - previous.time, 0.0001F);
        const float alpha = std::clamp((seconds - previous.time) / duration, 0.0F, 1.0F);
        Transform result;
        result.location =
            previous.transform.location +
            (next.transform.location - previous.transform.location) * alpha;
        result.rotationDegrees =
            previous.transform.rotationDegrees +
            (next.transform.rotationDegrees - previous.transform.rotationDegrees) * alpha;
        result.scale =
            previous.transform.scale +
            (next.transform.scale - previous.transform.scale) * alpha;
        return result;
    }
    return track.keys.back().transform;
}

void SkeletalAnimationComponent::rebuildClip() {
    tracks_.clear();
    length_ = 0.0F;
    playbackTime_ = 0.0F;
    appliedPoseCount_ = 0;
    loop_ = true;
    if (clipJson_.empty()) {
        return;
    }

    try {
        const Json clip = Json::parse(clipJson_);
        length_ = std::max(clip.value("length", 0.0F), 0.0F);
        loop_ = clip.value("loop", true);

        for (const Json& trackJson : clip.value("tracks", Json::array())) {
            Track track;
            track.bone = trackJson.value("bone", std::string{});
            if (track.bone.empty()) {
                continue;
            }
            for (const Json& keyJson : trackJson.value("keys", Json::array())) {
                Keyframe key;
                key.time = std::max(keyJson.value("time", 0.0F), 0.0F);
                key.transform = transformFromKeyJson(keyJson);
                track.keys.push_back(key);
                length_ = std::max(length_, key.time);
            }
            std::sort(
                track.keys.begin(),
                track.keys.end(),
                [](const Keyframe& first, const Keyframe& second) {
                    return first.time < second.time;
                }
            );
            if (!track.keys.empty()) {
                tracks_.push_back(std::move(track));
            }
        }
    } catch (const std::exception&) {
        tracks_.clear();
        length_ = 0.0F;
    }
}

BlueprintComponent::BlueprintComponent(std::string name, Actor* owner)
    : ActorComponent(std::move(name), owner) {
    tickSettings().enabled = true;
    tickSettings().group = TickGroup::PostUpdate;
}

std::string_view BlueprintComponent::typeName() const {
    return "BlueprintComponent";
}

void BlueprintComponent::beginPlay() {
    (void)executeEvent("BeginPlay");
}

void BlueprintComponent::tickComponent(float deltaTime) {
    (void)executeEvent("Tick", deltaTime);
}

const std::string& BlueprintComponent::graphJson() const {
    return graphJson_;
}

void BlueprintComponent::setGraphJson(std::string graph) {
    graphJson_ = std::move(graph);
    rebuildActions();
}

int BlueprintComponent::executionCount() const {
    return executionCount_;
}

int BlueprintComponent::executeEvent(
    std::string_view eventName,
    float deltaTime
) {
    rebuildActions();
    int executed = 0;
    for (const Action& action : actions_) {
        if (action.eventName == eventName && applyAction(action, deltaTime)) {
            ++executed;
            ++executionCount_;
        }
    }
    return executed;
}

Object* BlueprintComponent::resolveTarget(const Action& action) const {
    Actor* actor = owner();
    if (actor == nullptr) {
        return nullptr;
    }
    if (action.target.empty() || action.target == "Owner") {
        return actor;
    }

    for (const auto& component : actor->components()) {
        if (component->name() == action.target ||
            component->typeName() == action.target) {
            return component.get();
        }
        const TypeDescriptor* type = component->typeDescriptor();
        const TypeDescriptor* requested =
            ReflectionRegistry::instance().find(action.target);
        if (type != nullptr && requested != nullptr && type->isA(*requested)) {
            return component.get();
        }
    }
    return nullptr;
}

bool BlueprintComponent::applyAction(
    const Action& action,
    float deltaTime
) {
    Object* target = resolveTarget(action);
    if (target == nullptr) {
        return false;
    }
    const PropertyDescriptor* property = findProperty(*target, action.property);
    if (property == nullptr || !property->setter) {
        return false;
    }

    if (action.action == "SetProperty") {
        return property->setter(*target, action.value);
    }

    if (action.action == "AddFloat") {
        if (!property->getter || property->type != PropertyType::Float) {
            return false;
        }
        const PropertyValue currentValue = property->getter(*target);
        const auto* current = std::get_if<float>(&currentValue);
        const auto* amount = std::get_if<float>(&action.value);
        if (current == nullptr || amount == nullptr) {
            return false;
        }
        const float scale = action.scaleByDelta ? deltaTime : 1.0F;
        return property->setter(*target, *current + (*amount * scale));
    }

    return false;
}

void BlueprintComponent::rebuildActions() {
    actions_.clear();
    if (graphJson_.empty()) {
        return;
    }

    try {
        const Json graph = Json::parse(graphJson_);
        for (const Json& node : graph.value("nodes", Json::array())) {
            Action action;
            action.eventName = node.value("event", std::string{});
            action.action = node.value("action", std::string{});
            action.target = node.value("target", std::string{"Owner"});
            action.property = node.value("property", std::string{});
            action.scaleByDelta = node.value("scaleByDelta", false);

            Object* target = resolveTarget(action);
            const PropertyDescriptor* property =
                target == nullptr ? nullptr : findProperty(*target, action.property);
            if (property == nullptr || !node.contains("value")) {
                continue;
            }
            action.value = blueprintValueFromJson(property->type, node.at("value"));
            actions_.push_back(std::move(action));
        }
    } catch (const std::exception&) {
        actions_.clear();
    }
}

GameInstance::GameInstance()
    : Object("GameInstance", nullptr) {}

std::string_view GameInstance::typeName() const {
    return "GameInstance";
}

EngineRuntime::EngineRuntime() {
    input_.bindAxis("MoveForward", Key::W, Key::S);
    input_.bindAxis("MoveRight", Key::D, Key::A);
}

void EngineRuntime::setEditWorld(std::unique_ptr<World> world) {
    stop();
    editWorld_ = std::move(world);
    if (editWorld_ != nullptr) {
        connectInput(*editWorld_);
    }
    transactions_.clear();
}

World& EngineRuntime::editWorld() {
    if (editWorld_ == nullptr) {
        throw std::runtime_error("EngineRuntime has no edit World.");
    }
    return *editWorld_;
}

const World& EngineRuntime::editWorld() const {
    if (editWorld_ == nullptr) {
        throw std::runtime_error("EngineRuntime has no edit World.");
    }
    return *editWorld_;
}

World& EngineRuntime::activeWorld() {
    if (mode_ == EditorMode::PlayInEditor && playWorld_ != nullptr) {
        return *playWorld_;
    }
    return editWorld();
}

const World& EngineRuntime::activeWorld() const {
    if (mode_ == EditorMode::PlayInEditor && playWorld_ != nullptr) {
        return *playWorld_;
    }
    if (editWorld_ == nullptr) {
        throw std::runtime_error("EngineRuntime has no active World.");
    }
    return *editWorld_;
}

bool EngineRuntime::startSimulate() {
    if (editWorld_ == nullptr || mode_ != EditorMode::Edit) {
        return false;
    }
    mode_ = EditorMode::Simulate;
    editWorld_->beginPlay();
    log_.write("Simulate started.");
    return true;
}

bool EngineRuntime::startPlayInEditor() {
    if (editWorld_ == nullptr || mode_ != EditorMode::Edit) {
        return false;
    }
    // 직렬화 경계를 이용해 Edit World와 포인터를 공유하지 않는 완전한 복제본을
    // 만든다. PIE 종료 시 이 복제본만 폐기하므로 편집 값이 보존된다.
    playWorld_ = WorldSerializer::fromJson(
        WorldSerializer::toJson(*editWorld_),
        &log_
    );
    if (playWorld_ == nullptr) {
        return false;
    }
    connectInput(*playWorld_);

    PlayerController* controller = nullptr;
    Pawn* pawn = nullptr;
    for (const auto& actor : playWorld_->actors()) {
        if (controller == nullptr) {
            controller = dynamic_cast<PlayerController*>(actor.get());
        }
        if (pawn == nullptr) {
            pawn = dynamic_cast<Pawn*>(actor.get());
        }
    }
    if (controller != nullptr) {
        controller->possess(pawn);
    }

    mode_ = EditorMode::PlayInEditor;
    playWorld_->beginPlay();
    log_.write("Play In Editor started from a cloned World.");
    return true;
}

void EngineRuntime::stop() {
    if (mode_ == EditorMode::Simulate && editWorld_ != nullptr) {
        editWorld_->endPlay();
    }
    if (playWorld_ != nullptr) {
        playWorld_->endPlay();
        playWorld_.reset();
    }
    if (mode_ != EditorMode::Edit) {
        log_.write("Play stopped.");
    }
    mode_ = EditorMode::Edit;
}

void EngineRuntime::tick(float deltaTime) {
    if (mode_ != EditorMode::Edit) {
        activeWorld().tick(deltaTime);
    }
}

EditorMode EngineRuntime::mode() const {
    return mode_;
}

InputSystem& EngineRuntime::input() {
    return input_;
}

OutputLog& EngineRuntime::log() {
    return log_;
}

AssetRegistry& EngineRuntime::assets() {
    return assets_;
}

TransactionStack& EngineRuntime::transactions() {
    return transactions_;
}

void EngineRuntime::connectInput(World& world) {
    world.setInputSystem(&input_);
}

} // namespace engine
