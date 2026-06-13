#include "engine/Gameplay.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float first, float second) {
    return std::abs(first - second) < 0.001F;
}

engine::Actor& addBoxActor(
    engine::World& world,
    const char* name,
    glm::vec3 location,
    engine::CollisionChannel channel
) {
    auto& actor = world.spawnActor<engine::Actor>(name);
    auto& box = actor.addComponent<engine::BoxComponent>("Box");
    box.setRelativeLocation(location);
    box.setExtent({50.0F, 50.0F, 50.0F});
    box.setObjectChannel(channel);
    actor.setRootComponent(&box);
    return actor;
}

void testGuid() {
    const engine::Guid guid = engine::Guid::create();
    require(guid.valid(), "Created GUID must be valid.");
    require(
        engine::Guid::parse(guid.toString()).value() == guid,
        "GUID text roundtrip failed."
    );
}

void testAttachmentAndCycle() {
    engine::World world;
    auto& actor = world.spawnActor<engine::Actor>("Actor");
    auto& root = actor.addComponent<engine::SceneComponent>("Root");
    auto& child = actor.addComponent<engine::SceneComponent>("Child");
    auto& grandchild = actor.addComponent<engine::SceneComponent>("Grandchild");
    actor.setRootComponent(&root);
    require(child.attachTo(&root), "Child attachment failed.");
    require(grandchild.attachTo(&child), "Grandchild attachment failed.");
    require(!root.attachTo(&grandchild), "Attachment cycle was accepted.");

    root.setRelativeLocation({100.0F, 0.0F, 0.0F});
    child.setRelativeLocation({0.0F, 50.0F, 0.0F});
    const glm::vec3 location = child.worldTransform().location;
    require(
        near(location.x, 100.0F) && near(location.y, 50.0F),
        "World transform composition failed."
    );
}

void testCollision() {
    engine::World world;
    auto& movingActor = addBoxActor(
        world,
        "Moving",
        {0.0F, 0.0F, 50.0F},
        engine::CollisionChannel::Pawn
    );
    addBoxActor(
        world,
        "Wall",
        {120.0F, 0.0F, 50.0F},
        engine::CollisionChannel::WorldStatic
    );
    auto* moving = movingActor.findComponent<engine::BoxComponent>();
    const engine::MovementResult result =
        world.collision().moveComponent(*moving, {100.0F, 30.0F, 0.0F}, world);
    require(result.blockedX, "Blocking collision did not stop X movement.");
    require(near(result.location.y, 30.0F), "Slide movement did not preserve Y.");

    const auto hit = world.collision().raycast(
        {-300.0F, 0.0F, 50.0F},
        {1.0F, 0.0F, 0.0F},
        1000.0F,
        world
    );
    require(hit.has_value(), "Raycast missed known boxes.");

    const auto sweep = world.collision().sweep(
        {{-200.0F, 0.0F, 50.0F}, {25.0F, 25.0F, 25.0F}},
        {500.0F, 0.0F, 0.0F},
        world
    );
    require(sweep.has_value(), "Sweep missed known boxes.");
}

void testReflectionAndSerialization() {
    engine::registerEngineTypes();
    engine::World world("TestWorld");
    auto& controller =
        world.spawnActor<engine::PlayerController>("Controller");
    auto& character = world.spawnActor<engine::Character>("Hero");
    controller.possess(&character);
    character.setMoveSpeed(525.0F);
    const engine::Guid originalGuid = character.guid();

    const std::string text = engine::WorldSerializer::toJson(world);
    auto loaded = engine::WorldSerializer::fromJson(text);
    require(loaded != nullptr, "World JSON load failed.");
    auto* loadedCharacter =
        dynamic_cast<engine::Character*>(loaded->findActor(originalGuid));
    require(loadedCharacter != nullptr, "Actor GUID/type was not preserved.");
    require(
        near(loadedCharacter->moveSpeed(), 525.0F),
        "Reflected property was not preserved."
    );
    require(
        loadedCharacter->findComponent<engine::BoxComponent>() != nullptr,
        "Component hierarchy was not preserved."
    );
    require(
        loadedCharacter->findComponent<engine::BoxComponent>()->objectChannel() ==
            engine::CollisionChannel::Pawn,
        "Collision channel was not preserved."
    );
    engine::PlayerController* loadedController = nullptr;
    for (const auto& actor : loaded->actors()) {
        if (auto* candidate =
                dynamic_cast<engine::PlayerController*>(actor.get())) {
            loadedController = candidate;
        }
    }
    require(
        loadedController != nullptr && loadedController->pawn() == loadedCharacter,
        "Controller possession was not preserved."
    );
}

void testPieIsolation() {
    engine::EngineRuntime runtime;
    auto world = std::make_unique<engine::World>("Edit");
    auto& character = world->spawnActor<engine::Character>("Hero");
    const engine::Guid guid = character.guid();
    runtime.setEditWorld(std::move(world));
    require(runtime.startPlayInEditor(), "PIE did not start.");
    auto* playCharacter =
        dynamic_cast<engine::Character*>(runtime.activeWorld().findActor(guid));
    playCharacter->rootComponent()->setRelativeLocation({999.0F, 0.0F, 45.0F});
    runtime.stop();
    const auto* editCharacter =
        dynamic_cast<const engine::Character*>(runtime.editWorld().findActor(guid));
    require(
        !near(editCharacter->actorTransform().location.x, 999.0F),
        "PIE modified the edit World."
    );
}

void testRenderProxyDirtyUpdate() {
    engine::World world;
    auto& actor = world.spawnActor<engine::Actor>("Visible");
    auto& mesh = actor.addComponent<engine::StaticMeshComponent>("Mesh");
    actor.setRootComponent(&mesh);

    world.renderScene().sync(world);
    require(
        world.renderScene().updatesLastSync() == 1,
        "Initial proxy was not collected."
    );
    world.renderScene().sync(world);
    require(
        world.renderScene().updatesLastSync() == 0,
        "Unchanged proxy was updated again."
    );
    mesh.setRelativeLocation({10.0F, 0.0F, 0.0F});
    world.renderScene().sync(world);
    require(
        world.renderScene().updatesLastSync() == 1,
        "Dirty transform did not update proxy."
    );
}

void testCharacterCameraSeesPlayer() {
    engine::World world;
    auto& character = world.spawnActor<engine::Character>("Player");
    character.rootComponent()->setRelativeLocation({0.0F, 250.0F, 45.0F});
    const auto* camera = character.findComponent<engine::CameraComponent>();
    require(camera != nullptr, "Character camera was not constructed.");

    const engine::CameraView view = camera->cameraView(16.0F / 9.0F);
    const glm::vec4 clip = view.viewProjection() * glm::vec4{
        character.actorTransform().location,
        1.0F,
    };
    require(clip.w > 0.0F, "Player is behind the Character camera.");
    const glm::vec3 ndc = glm::vec3{clip} / clip.w;
    require(
        std::abs(ndc.x) < 1.0F && std::abs(ndc.y) < 1.0F,
        "Player is outside the Character camera viewport."
    );
}

} // namespace

int main() {
    try {
        testGuid();
        testAttachmentAndCycle();
        testCollision();
        testReflectionAndSerialization();
        testPieIsolation();
        testRenderProxyDirtyUpdate();
        testCharacterCameraSeesPlayer();
        std::cout << "All engine tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return 1;
    }
}
