#include "engine/Gameplay.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <stdexcept>

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

    auto& mesh = addComponent<StaticMeshComponent>("PlayerMesh");
    mesh.attachTo(collision_);
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
    lastMovement_ = world()->collision().moveComponent(
        *collision_,
        delta,
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
