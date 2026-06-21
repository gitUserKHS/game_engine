#include "engine/Gameplay.hpp"
#include "engine/Editor.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/matrix.hpp>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float first, float second) {
    return std::abs(first - second) < 0.001F;
}

bool near(const glm::vec3& first, const glm::vec3& second) {
    return glm::length(first - second) < 0.01F;
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

void writeTextFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream << text;
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

void testSteppedMovementClimbsLowObstacle() {
    engine::World world;
    auto& mover = addBoxActor(
        world,
        "Mover",
        {0.0F, 0.0F, 45.0F},
        engine::CollisionChannel::Pawn
    );
    auto* moving = mover.findComponent<engine::BoxComponent>();
    moving->setExtent({45.0F, 45.0F, 45.0F});
    auto& step = addBoxActor(
        world,
        "Step",
        {80.0F, 0.0F, 10.0F},
        engine::CollisionChannel::WorldStatic
    );
    step.findComponent<engine::BoxComponent>()->setExtent({25.0F, 60.0F, 10.0F});

    const engine::MovementResult result = world.collision().moveComponentStepped(
        *moving,
        {100.0F, 0.0F, 0.0F},
        35.0F,
        world
    );
    require(
        result.location.x > 90.0F && result.location.z > 45.0F,
        "Stepped movement did not climb a low obstacle."
    );
}

void testInputConfigLoadsAxisAndActions() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cocoa-engine-input-test";
    std::filesystem::remove_all(root);
    const std::filesystem::path config = root / "default.input.json";
    writeTextFile(
        config,
        R"({
  "axes": [
    {"name": "MoveForward", "positive": "W", "negative": "S"},
    {"name": "MoveRight", "positive": "D", "negative": "A"}
  ],
  "actions": [
    {"name": "Jump", "key": "Space"}
  ]
})"
    );

    engine::OutputLog log;
    engine::InputSystem input;
    require(input.loadConfig(config, &log), "Input config did not load.");
    input.setKeyDown(engine::Key::W, true);
    input.setKeyDown(engine::Key::A, true);
    input.setKeyDown(engine::Key::Space, true);
    require(
        near(input.axis("MoveForward"), 1.0F) &&
            near(input.axis("MoveRight"), -1.0F) &&
            input.action("Jump"),
        "Input config bindings did not produce expected values."
    );

    std::filesystem::remove_all(root);
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
    auto& debugActor = world.spawnActor<engine::Actor>("DebugBox");
    auto& debugBox = debugActor.addComponent<engine::BoxComponent>("Box");
    debugBox.setDrawDebug(true);
    debugActor.setRootComponent(&debugBox);

    world.renderScene().sync(world);
    require(
        world.renderScene().updatesLastSync() == 2,
        "Initial proxy was not collected."
    );
    require(
        world.renderScene().opaqueProxyCount() == 1 &&
            world.renderScene().debugWireProxyCount() == 1,
        "RenderScene did not classify opaque and debug wire proxies."
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

    engine::MaterialInstance material;
    material.baseColor = {0.9F, 0.2F, 0.1F};
    mesh.setMaterial(material);
    world.renderScene().sync(world);
    const auto meshProxy = std::find_if(
        world.renderScene().proxies().begin(),
        world.renderScene().proxies().end(),
        [&](const engine::RenderProxy& proxy) {
            return proxy.componentGuid == mesh.guid();
        }
    );
    require(
        world.renderScene().updatesLastSync() == 1 &&
            meshProxy != world.renderScene().proxies().end() &&
            near(meshProxy->material.baseColor, material.baseColor),
        "Material edits did not update the render proxy."
    );
}

void testDirectionalLightProxy() {
    engine::World world;
    auto& actor = world.spawnActor<engine::Actor>("Sun");
    auto& light = actor.addComponent<engine::DirectionalLightComponent>("Light");
    actor.setRootComponent(&light);
    light.setRelativeRotation({0.0F, -45.0F, -35.0F});
    light.setColor({0.8F, 0.7F, 0.6F});
    light.setIntensity(2.0F);

    world.renderScene().sync(world);
    require(
        world.renderScene().lights().size() == 1,
        "Directional light was not collected by RenderScene."
    );
    const engine::DirectionalLightProxy& proxy = world.renderScene().lights().front();
    require(
        proxy.componentGuid == light.guid() &&
            near(glm::length(proxy.direction), 1.0F) &&
            near(proxy.color, {0.8F, 0.7F, 0.6F}) &&
            near(proxy.intensity, 2.0F),
        "Directional light proxy values were not preserved."
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

void testSpringArmCameraCollisionPullsCameraIn() {
    engine::World world;
    auto& character = world.spawnActor<engine::Character>("Player");
    auto* camera = character.findComponent<engine::CameraComponent>();
    require(camera != nullptr, "Character camera was not constructed.");

    const glm::vec3 target = character.rootComponent()->worldTransform().location;
    const glm::vec3 desired = camera->worldTransform().location - target;
    const float originalDistance = glm::length(desired);
    auto& wall = addBoxActor(
        world,
        "CameraBlocker",
        target + glm::normalize(desired) * 300.0F,
        engine::CollisionChannel::WorldStatic
    );
    wall.findComponent<engine::BoxComponent>()->setExtent({120.0F, 120.0F, 120.0F});

    const engine::CameraView view = camera->cameraView(16.0F / 9.0F);
    const glm::vec3 resolvedLocation = glm::vec3{glm::inverse(view.view)[3]};
    require(
        glm::distance(target, resolvedLocation) < originalDistance,
        "SpringArm camera collision did not pull the camera closer."
    );
}

void testCombatProjectileDamagesHealth() {
    engine::World world;
    auto& shooter = world.spawnActor<engine::Actor>("Shooter");
    auto& shooterRoot = shooter.addComponent<engine::SceneComponent>("Root");
    shooter.setRootComponent(&shooterRoot);
    auto& combat = shooter.addComponent<engine::CombatComponent>("Combat");
    combat.setProjectileDamage(35.0F);
    combat.setProjectileSpeed(1000.0F);

    auto& target = addBoxActor(
        world,
        "Target",
        {200.0F, 0.0F, 45.0F},
        engine::CollisionChannel::Pawn
    );
    auto& health = target.addComponent<engine::HealthComponent>("Health");
    health.setMaxHealth(100.0F);
    health.setCurrentHealth(100.0F);

    engine::Actor* projectile = combat.fireProjectile({1.0F, 0.0F, 0.0F});
    require(projectile != nullptr, "CombatComponent did not spawn a projectile.");
    world.tick(0.2F);
    require(
        near(health.currentHealth(), 65.0F),
        "Projectile did not damage the target HealthComponent."
    );
}

void testBlueprintLiteEventsAndSerialization() {
    engine::registerEngineTypes();
    engine::World world;
    auto& character = world.spawnActor<engine::Character>("ScriptedHero");
    auto& health = character.addComponent<engine::HealthComponent>("Health");
    health.setMaxHealth(100.0F);
    health.setCurrentHealth(100.0F);
    auto& blueprint =
        character.addComponent<engine::BlueprintComponent>("Blueprint");
    blueprint.setGraphJson(R"({
  "nodes": [
    {
      "event": "BeginPlay",
      "action": "SetProperty",
      "target": "Owner",
      "property": "MoveSpeed",
      "value": 720.0
    },
    {
      "event": "Tick",
      "action": "AddFloat",
      "target": "HealthComponent",
      "property": "CurrentHealth",
      "value": -10.0,
      "scaleByDelta": true
    }
  ]
})");

    world.beginPlay();
    require(
        near(character.moveSpeed(), 720.0F),
        "Blueprint BeginPlay did not set an owner property."
    );
    world.tick(0.5F);
    require(
        near(health.currentHealth(), 95.0F),
        "Blueprint Tick did not add to a component float property."
    );
    require(
        blueprint.executionCount() >= 2,
        "Blueprint execution count did not track executed nodes."
    );

    const std::string saved = engine::WorldSerializer::toJson(world);
    auto loaded = engine::WorldSerializer::fromJson(saved);
    require(loaded != nullptr, "Blueprint World JSON did not reload.");
    auto* loadedCharacter =
        dynamic_cast<engine::Character*>(loaded->findActor(character.guid()));
    require(
        loadedCharacter != nullptr,
        "Blueprint owner Actor was not restored."
    );
    loadedCharacter->setMoveSpeed(300.0F);
    loaded->beginPlay();
    require(
        near(loadedCharacter->moveSpeed(), 720.0F),
        "Blueprint GraphJson did not survive serialization."
    );
}

void testEditorViewportMath() {
    engine::EditorViewportController camera;
    const glm::vec3 originalPosition = camera.position();
    const glm::vec3 expectedDirection =
        glm::normalize(camera.pivot() - camera.position());
    const engine::EditorRay centerRay =
        camera.screenRay({400.0F, 300.0F}, {800.0F, 600.0F});
    require(
        near(centerRay.direction, expectedDirection),
        "Center screen ray does not follow the editor camera."
    );

    engine::EditorViewportInput input;
    input.deltaTime = 0.25F;
    input.look = true;
    input.moveForward = true;
    input.mouseDelta = {50.0F, 10000.0F};
    camera.update(input);
    require(
        camera.pitchDegrees() >= -89.0F &&
            camera.pitchDegrees() <= 89.0F,
        "Editor camera pitch was not clamped."
    );
    require(
        !near(camera.position(), originalPosition),
        "Editor camera did not move with delta-time input."
    );

    const glm::vec3 focusPoint{100.0F, 200.0F, 300.0F};
    const glm::vec3 pivotOnly{25.0F, 50.0F, 75.0F};
    const glm::vec3 positionBeforePivot = camera.position();
    camera.setPivot(pivotOnly);
    require(
        near(camera.pivot(), pivotOnly) &&
            near(camera.position(), positionBeforePivot),
        "Changing the orbit pivot moved the editor camera."
    );
    camera.focus(focusPoint, 80.0F);
    require(
        near(camera.pivot(), focusPoint),
        "Editor camera focus did not update its pivot."
    );
}

void testRenderProxyPicking() {
    engine::World world;
    auto& nearActor = world.spawnActor<engine::Actor>("Near");
    auto& nearMesh =
        nearActor.addComponent<engine::StaticMeshComponent>("Mesh");
    engine::Transform nearTransform;
    nearTransform.location = {200.0F, 0.0F, 0.0F};
    nearTransform.scale = {100.0F, 100.0F, 100.0F};
    nearMesh.setRelativeTransform(nearTransform);
    nearActor.setRootComponent(&nearMesh);

    auto& farActor = world.spawnActor<engine::Actor>("Far");
    auto& farMesh =
        farActor.addComponent<engine::StaticMeshComponent>("Mesh");
    engine::Transform farTransform = nearTransform;
    farTransform.location.x = 400.0F;
    farMesh.setRelativeTransform(farTransform);
    farActor.setRootComponent(&farMesh);
    world.renderScene().sync(world);

    const auto hit = engine::pickRenderProxy(
        world.renderScene(),
        {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}
    );
    require(hit.has_value(), "Render proxy picking missed visible cubes.");
    require(
        hit->componentGuid == nearMesh.guid(),
        "Render proxy picking did not choose the nearest cube."
    );
    require(
        !engine::pickRenderProxy(
             world.renderScene(),
             {{0.0F, 1000.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}
         ).has_value(),
        "Render proxy picking hit empty space."
    );
}

void testWorldRestoreAndSnapshotTransactions() {
    engine::World world("SnapshotWorld");
    engine::World* originalAddress = &world;
    auto& controller =
        world.spawnActor<engine::PlayerController>("Controller");
    auto& character = world.spawnActor<engine::Character>("Hero");
    const engine::Guid controllerGuid = controller.guid();
    const engine::Guid characterGuid = character.guid();
    controller.possess(&character);

    const std::string before = engine::WorldSerializer::toJson(world);
    auto& added = world.spawnActor<engine::Actor>("Added");
    auto& root = added.addComponent<engine::SceneComponent>("Root");
    added.setRootComponent(&root);
    const engine::Guid addedGuid = added.guid();
    const std::string after = engine::WorldSerializer::toJson(world);

    engine::TransactionStack transactions;
    transactions.recordSnapshot(before, after);
    require(transactions.undo(world), "Snapshot undo failed.");
    require(&world == originalAddress, "Snapshot restore replaced the World.");
    require(
        world.findActor(addedGuid) == nullptr,
        "Snapshot undo kept a newly created Actor."
    );
    auto* restoredController = dynamic_cast<engine::PlayerController*>(
        world.findActor(controllerGuid)
    );
    require(
        restoredController != nullptr &&
            restoredController->pawn() == world.findActor(characterGuid),
        "Snapshot restore lost Controller possession."
    );

    require(transactions.redo(world), "Snapshot redo failed.");
    require(
        world.findActor(addedGuid) != nullptr,
        "Snapshot redo did not recreate the Actor."
    );

    auto* restoredCharacter =
        dynamic_cast<engine::Character*>(world.findActor(characterGuid));
    auto* duplicate = engine::WorldSerializer::duplicateActor(
        world,
        *restoredCharacter,
        "Hero 2"
    );
    require(duplicate != nullptr, "Actor duplication failed.");
    require(
        duplicate->guid() != restoredCharacter->guid(),
        "Duplicated Actor reused its source GUID."
    );
    require(
        duplicate->rootComponent() != nullptr &&
            duplicate->rootComponent()->guid() !=
                restoredCharacter->rootComponent()->guid(),
        "Duplicated components reused source GUIDs."
    );
    for (const auto& component : duplicate->components()) {
        require(
            component->registered(),
            "Duplicated component was not registered in its new World."
        );
    }

    restoredController = dynamic_cast<engine::PlayerController*>(
        world.findActor(controllerGuid)
    );
    world.destroyActor(*restoredCharacter);
    require(
        restoredController->pawn() == nullptr,
        "Destroying a possessed Pawn did not unpossess it first."
    );
}

void testTransformTransactionRestoresRotationAndScale() {
    engine::registerEngineTypes();

    engine::World world("TransformTransactionWorld");
    auto& actor = world.spawnActor<engine::Actor>("Editable");
    auto& root = actor.addComponent<engine::SceneComponent>("Root");
    actor.setRootComponent(&root);

    engine::Transform before;
    before.location = {10.0F, 20.0F, 30.0F};
    before.rotationDegrees = {0.0F, 15.0F, 30.0F};
    before.scale = {1.0F, 1.5F, 2.0F};
    root.setRelativeTransform(before);

    engine::Transform after = before;
    after.location = {100.0F, -50.0F, 75.0F};
    after.rotationDegrees = {45.0F, 90.0F, 135.0F};
    after.scale = {2.0F, 0.5F, 3.0F};
    root.setRelativeTransform(after);

    engine::TransactionStack transactions;
    transactions.recordTransform(root.guid(), before, after);

    require(transactions.undo(world), "Transform transaction undo failed.");
    require(
        near(root.relativeTransform().location, before.location) &&
            near(root.relativeTransform().rotationDegrees, before.rotationDegrees) &&
            near(root.relativeTransform().scale, before.scale),
        "Transform undo did not restore location, rotation, and scale."
    );

    require(transactions.redo(world), "Transform transaction redo failed.");
    require(
        near(root.relativeTransform().location, after.location) &&
            near(root.relativeTransform().rotationDegrees, after.rotationDegrees) &&
            near(root.relativeTransform().scale, after.scale),
        "Transform redo did not reapply location, rotation, and scale."
    );
}

void testApplicationOptions() {
    const std::array<std::string_view, 9> arguments{
        "--capture",
        "Saved/Screenshots/test.png",
        "--capture-target",
        "both",
        "--capture-frame",
        "7",
        "--exit-after-capture",
        "--window-size",
        "1280x720",
    };
    const engine::ApplicationOptions options =
        engine::parseApplicationOptions(arguments);
    require(options.screenshot.has_value(), "CLI capture was not parsed.");
    require(
        options.screenshot->target == engine::ScreenshotTarget::Both &&
            options.screenshot->frame == 7 &&
            options.screenshot->exitAfterCapture,
        "CLI screenshot options were parsed incorrectly."
    );
    require(
        options.windowWidth == 1280 && options.windowHeight == 720,
        "CLI window size was parsed incorrectly."
    );

    bool rejected = false;
    try {
        const std::array<std::string_view, 2> invalid{
            "--window-size",
            "bad-size",
        };
        static_cast<void>(engine::parseApplicationOptions(invalid));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Invalid CLI window size was accepted.");
}

void testPngWriter() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "cocoa-engine-png-test.png";
    const std::array<unsigned char, 16> bottomLeftPixels{
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };
    std::string error;
    require(
        engine::writePngRgba(
            path,
            2,
            2,
            bottomLeftPixels,
            true,
            &error
        ),
        "PNG writer failed."
    );

    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 8> signature{};
    stream.read(
        reinterpret_cast<char*>(signature.data()),
        static_cast<std::streamsize>(signature.size())
    );
    const std::array<unsigned char, 8> expectedSignature{
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    };
    require(signature == expectedSignature, "PNG signature is invalid.");

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load(
        path.string().c_str(),
        &width,
        &height,
        &channels,
        4
    );
    require(
        decoded != nullptr && width == 2 && height == 2,
        "PNG dimensions could not be decoded."
    );
    require(
        decoded[0] == 0 && decoded[1] == 0 && decoded[2] == 255,
        "Bottom-left RGBA data was not flipped for PNG coordinates."
    );
    stbi_image_free(decoded);
    stream.close();
    std::filesystem::remove(path);
}

void testAssetRegistryLoadsGuidAssets() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cocoa-engine-assets-test";
    std::filesystem::remove_all(root);

    const engine::Guid meshGuid =
        engine::Guid::parse("11111111111111112222222222222222").value();
    const engine::Guid materialGuid =
        engine::Guid::parse("33333333333333334444444444444444").value();

    writeTextFile(
        root / "Meshes" / "Cube.asset.json",
        R"({"type":"StaticMesh","primitive":"Cube"})"
    );
    writeTextFile(
        root / "Meshes" / "Cube.asset.json.meta",
        R"({"guid":"11111111111111112222222222222222","name":"Cube","type":"StaticMesh","source":"Cube.asset.json"})"
    );
    writeTextFile(
        root / "Materials" / "Default.material.json",
        R"({"baseColor":[0.25,0.5,0.75],"roughness":0.4,"metallic":0.1})"
    );
    writeTextFile(
        root / "Materials" / "Default.material.json.meta",
        R"({"guid":"33333333333333334444444444444444","name":"Default","type":"Material","source":"Default.material.json"})"
    );

    engine::OutputLog log;
    engine::AssetRegistry registry;
    registry.scan(root, &log);
    require(registry.assets().size() == 2, "Asset registry did not scan meta files.");
    require(
        registry.find(meshGuid) != nullptr &&
            registry.findByName("Default") != nullptr &&
            registry.findByType("StaticMesh").size() == 1,
        "Asset lookup by GUID, name, or type failed."
    );

    const auto mesh = registry.loadStaticMesh(meshGuid, &log);
    require(
        mesh.has_value() && mesh->guid == meshGuid &&
            mesh->primitive == engine::MeshPrimitive::Cube,
        "StaticMesh asset did not load from GUID metadata."
    );

    const auto material = registry.loadMaterial(materialGuid, &log);
    require(
        material.has_value() &&
            near(material->material.baseColor, {0.25F, 0.5F, 0.75F}) &&
            near(material->material.roughness, 0.4F) &&
            near(material->material.metallic, 0.1F),
        "Material asset did not load reflected values."
    );

    std::filesystem::remove_all(root);
}

void testAssetImporterCreatesMetaFiles() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cocoa-engine-import-test";
    const std::filesystem::path external =
        std::filesystem::temp_directory_path() / "cocoa-engine-import-source";
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(external);

    const std::filesystem::path gltf = external / "Props" / "Crate.gltf";
    const std::filesystem::path texture = external / "Textures" / "Crate.png";
    writeTextFile(
        gltf,
        R"({"asset":{"version":"2.0","generator":"engine-test"},"scenes":[]})"
    );
    writeTextFile(texture, "not-a-real-png-yet");

    engine::OutputLog log;
    const engine::AssetImportResult meshImport =
        engine::AssetImporter::importGltfAsStaticMesh(gltf, root, &log);
    require(meshImport.success, "glTF import did not create a StaticMesh asset.");
    require(
        std::filesystem::exists(meshImport.copiedSource) &&
            std::filesystem::exists(meshImport.asset.source) &&
            std::filesystem::exists(meshImport.metadata),
        "glTF import did not write expected files."
    );

    const engine::AssetImportResult textureImport =
        engine::AssetImporter::importTexture(texture, root, &log);
    require(textureImport.success, "Texture import did not create a Texture asset.");
    require(
        std::filesystem::exists(textureImport.copiedSource) &&
            std::filesystem::exists(textureImport.asset.source) &&
            std::filesystem::exists(textureImport.metadata),
        "Texture import did not write expected files."
    );

    engine::AssetRegistry registry;
    registry.scan(root, &log);
    require(
        registry.find(meshImport.asset.guid) != nullptr &&
            registry.find(textureImport.asset.guid) != nullptr,
        "Imported assets were not discoverable by GUID."
    );
    require(
        registry.findByType("StaticMesh").size() == 1 &&
            registry.findByType("Texture").size() == 1,
        "Imported asset types were not indexed."
    );
    require(
        registry.loadStaticMesh(meshImport.asset.guid, &log).has_value(),
        "Imported glTF-backed StaticMesh could not be loaded."
    );
    const auto textureAsset = registry.loadTexture(textureImport.asset.guid, &log);
    require(
        textureAsset.has_value() &&
            textureAsset->guid == textureImport.asset.guid &&
            std::filesystem::exists(textureAsset->source) &&
            textureAsset->sourceFormat == "png",
        "Imported Texture could not be loaded from GUID metadata."
    );

    std::filesystem::remove_all(root);
    std::filesystem::remove_all(external);
}

} // namespace

int main() {
    try {
        testGuid();
        testAttachmentAndCycle();
        testCollision();
        testSteppedMovementClimbsLowObstacle();
        testInputConfigLoadsAxisAndActions();
        testReflectionAndSerialization();
        testPieIsolation();
        testRenderProxyDirtyUpdate();
        testDirectionalLightProxy();
        testCharacterCameraSeesPlayer();
        testSpringArmCameraCollisionPullsCameraIn();
        testCombatProjectileDamagesHealth();
        testBlueprintLiteEventsAndSerialization();
        testEditorViewportMath();
        testRenderProxyPicking();
        testWorldRestoreAndSnapshotTransactions();
        testTransformTransactionRestoresRotationAndScale();
        testApplicationOptions();
        testPngWriter();
        testAssetRegistryLoadsGuidAssets();
        testAssetImporterCreatesMetaFiles();
        std::cout << "All engine tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return 1;
    }
}
