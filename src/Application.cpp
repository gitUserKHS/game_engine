#include "engine/Application.hpp"

#include "engine/Renderer.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef ENGINE_CONTENT_DIR
#define ENGINE_CONTENT_DIR "Content"
#endif

namespace {

constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 810;
constexpr float kFixedDeltaTime = 1.0F / 60.0F;
constexpr float kToolbarHeight = 48.0F;
constexpr float kLeftPanelWidth = 270.0F;
constexpr float kRightPanelWidth = 330.0F;
constexpr float kBottomPanelHeight = 220.0F;

constexpr ImGuiWindowFlags kFixedPanelFlags =
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse;

void placeWindow(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(
        {x, y},
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(
        {width, height},
        ImGuiCond_Always
    );
}

const char* modeName(engine::EditorMode mode) {
    switch (mode) {
    case engine::EditorMode::Edit:
        return "Edit";
    case engine::EditorMode::Simulate:
        return "Simulate";
    case engine::EditorMode::PlayInEditor:
        return "Play In Editor";
    }
    return "Unknown";
}

} // namespace

namespace engine {

Application::Application() {
    registerEngineTypes();
    createDemoWorld();
    runtime_.assets().scan(ENGINE_CONTENT_DIR, &runtime_.log());
}

Application::~Application() {
    shutdown();
}

int Application::run() {
    if (!initialize()) {
        return 1;
    }

    double previousTime = glfwGetTime();
    double accumulator = 0.0;
    while (!glfwWindowShouldClose(window_)) {
        const double currentTime = glfwGetTime();
        // 디버거 중단처럼 긴 공백이 생겨도 한 프레임에 0.25초보다 많이 따라잡지
        // 않게 제한한다. 게임 규칙은 아래 고정 60Hz update로만 전진한다.
        accumulator += std::min(currentTime - previousTime, 0.25);
        previousTime = currentTime;

        glfwPollEvents();
        processInput();
        while (accumulator >= kFixedDeltaTime) {
            fixedUpdate(kFixedDeltaTime);
            accumulator -= kFixedDeltaTime;
        }

        render();
        glfwSwapBuffers(window_);
    }
    return 0;
}

bool Application::initialize() {
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "GLFW initialization failed.\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(
        kWindowWidth,
        kWindowHeight,
        "Cocoa Engine - Unreal-inspired learning engine",
        nullptr,
        nullptr
    );
    if (window_ == nullptr) {
        std::cerr << "Window creation failed.\n";
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        std::cerr << "GLAD could not load OpenGL functions.\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    renderer_ = std::make_unique<Renderer>();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.ChildRounding = 3.0F;
    style.FrameRounding = 3.0F;
    style.WindowBorderSize = 1.0F;
    style.WindowPadding = {10.0F, 8.0F};
    style.ItemSpacing = {8.0F, 6.0F};
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    return true;
}

void Application::shutdown() {
    runtime_.stop();
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    renderer_.reset();
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void Application::processInput() {
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    const bool capture =
        ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
    const auto set = [this, capture](Key key, int glfwKey) {
        runtime_.input().setKeyDown(
            key,
            !capture && glfwGetKey(window_, glfwKey) == GLFW_PRESS
        );
    };
    set(Key::W, GLFW_KEY_W);
    set(Key::A, GLFW_KEY_A);
    set(Key::S, GLFW_KEY_S);
    set(Key::D, GLFW_KEY_D);

    const bool f5Down = glfwGetKey(window_, GLFW_KEY_F5) == GLFW_PRESS;
    if (!capture && f5Down && !f5WasDown_) {
        if (runtime_.mode() == EditorMode::Edit) {
            runtime_.startPlayInEditor();
        } else {
            runtime_.stop();
        }
        selectedGuid_ = {};
    }
    f5WasDown_ = f5Down;
}

void Application::fixedUpdate(float deltaTime) {
    runtime_.tick(deltaTime);
}

void Application::createDemoWorld() {
    auto world = std::make_unique<World>("DemoWorld");

    auto& controller = world->spawnActor<PlayerController>("PlayerController");
    auto& player = world->spawnActor<Character>("Player");
    player.rootComponent()->setRelativeLocation({0.0F, 250.0F, 45.0F});
    controller.possess(&player);
    selectedGuid_ = player.guid();

    auto createCube = [&world](
                          std::string name,
                          glm::vec3 location,
                          glm::vec3 size,
                          glm::vec3 color,
                          CollisionChannel channel
                      ) {
        Actor& actor = world->spawnActor<Actor>(std::move(name));
        auto& collision = actor.addComponent<BoxComponent>("Collision");
        collision.setRelativeLocation(location);
        collision.setExtent(size * 0.5F);
        collision.setObjectChannel(channel);
        collision.setDrawDebug(true);
        actor.setRootComponent(&collision);

        auto& mesh = actor.addComponent<StaticMeshComponent>("Mesh");
        mesh.attachTo(&collision);
        Transform meshTransform;
        meshTransform.scale = size;
        mesh.setRelativeTransform(meshTransform);
        MaterialInstance material;
        material.baseColor = color;
        mesh.setMaterial(material);
    };

    createCube(
        "Floor",
        {0.0F, 0.0F, -10.0F},
        {1600.0F, 1600.0F, 20.0F},
        {0.16F, 0.20F, 0.23F},
        CollisionChannel::WorldStatic
    );
    createCube(
        "Obstacle 1",
        {250.0F, 100.0F, 50.0F},
        {100.0F, 100.0F, 100.0F},
        {0.85F, 0.38F, 0.20F},
        CollisionChannel::WorldStatic
    );
    createCube(
        "Obstacle 2",
        {-240.0F, -100.0F, 75.0F},
        {150.0F, 150.0F, 150.0F},
        {0.85F, 0.38F, 0.20F},
        CollisionChannel::WorldStatic
    );
    createCube(
        "Obstacle 3",
        {50.0F, -350.0F, 60.0F},
        {120.0F, 120.0F, 120.0F},
        {0.85F, 0.38F, 0.20F},
        CollisionChannel::WorldStatic
    );

    Actor& lightActor = world->spawnActor<Actor>("Sun");
    auto& light = lightActor.addComponent<DirectionalLightComponent>("Light");
    light.setRelativeRotation({0.0F, -45.0F, -35.0F});
    lightActor.setRootComponent(&light);

    runtime_.setEditWorld(std::move(world));
    runtime_.log().write(
        "Demo World created. Z-up coordinates use centimeters."
    );
}

void Application::render() {
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    glfwGetWindowSize(window_, &windowWidth, &windowHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0 ||
        windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    updateEditorLayout(windowWidth, windowHeight);
    const float scaleX =
        static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);
    const float scaleY =
        static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight);
    const int viewportX = static_cast<int>(viewportRect_.x * scaleX);
    const int viewportY = static_cast<int>(
        (static_cast<float>(windowHeight) -
         viewportRect_.y - viewportRect_.height) * scaleY
    );
    const int viewportWidth =
        static_cast<int>(viewportRect_.width * scaleX);
    const int viewportHeight =
        static_cast<int>(viewportRect_.height * scaleY);

    World& world = runtime_.activeWorld();
    world.renderScene().sync(world);
    const CameraView camera = activeCamera(
        viewportRect_.width / std::max(viewportRect_.height, 1.0F)
    );
    renderer_->beginFrame(
        camera,
        framebufferWidth,
        framebufferHeight,
        viewportX,
        viewportY,
        viewportWidth,
        viewportHeight
    );
    renderer_->render(world.renderScene());

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawViewportPanel();
    drawToolbar();
    drawWorldOutliner();
    drawDetails();
    drawContentBrowser();
    drawOutputLog();
    drawDebugPanel();
    drawTransformGizmo(camera);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::updateEditorLayout(int width, int height) {
    const float screenWidth = static_cast<float>(width);
    const float screenHeight = static_cast<float>(height);
    const float leftWidth = std::min(
        kLeftPanelWidth,
        screenWidth * 0.24F
    );
    const float rightWidth = std::min(
        kRightPanelWidth,
        screenWidth * 0.28F
    );
    const float bottomHeight = std::min(
        kBottomPanelHeight,
        screenHeight * 0.30F
    );
    const float centerWidth =
        std::max(screenWidth - leftWidth - rightWidth, 160.0F);
    const float centerHeight = std::max(
        screenHeight - kToolbarHeight - bottomHeight,
        120.0F
    );

    toolbarRect_ = {0.0F, 0.0F, screenWidth, kToolbarHeight};
    outlinerRect_ = {
        0.0F,
        kToolbarHeight,
        leftWidth,
        centerHeight,
    };
    detailsRect_ = {
        screenWidth - rightWidth,
        kToolbarHeight,
        rightWidth,
        centerHeight,
    };
    contentRect_ = {
        0.0F,
        screenHeight - bottomHeight,
        leftWidth,
        bottomHeight,
    };
    outputRect_ = {
        leftWidth,
        screenHeight - bottomHeight,
        centerWidth,
        bottomHeight,
    };
    debugRect_ = {
        screenWidth - rightWidth,
        screenHeight - bottomHeight,
        rightWidth,
        bottomHeight,
    };
    viewportRect_ = {
        leftWidth,
        kToolbarHeight,
        centerWidth,
        centerHeight,
    };
}

void Application::drawViewportPanel() {
    placeWindow(
        viewportRect_.x,
        viewportRect_.y,
        viewportRect_.width,
        viewportRect_.height
    );
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Viewport", nullptr, flags);
    ImGui::GetWindowDrawList()->AddRect(
        {viewportRect_.x, viewportRect_.y},
        {
            viewportRect_.x + viewportRect_.width,
            viewportRect_.y + viewportRect_.height,
        },
        IM_COL32(88, 102, 120, 255)
    );
    ImGui::SetCursorPos({12.0F, 10.0F});
    ImGui::TextDisabled(
        "Viewport  |  %s  |  Z-up, centimeters",
        modeName(runtime_.mode())
    );
    ImGui::End();
}

void Application::drawToolbar() {
    placeWindow(
        toolbarRect_.x,
        toolbarRect_.y,
        toolbarRect_.width,
        toolbarRect_.height
    );
    constexpr ImGuiWindowFlags flags =
        kFixedPanelFlags |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Play Controls", nullptr, flags);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mode: %s", modeName(runtime_.mode()));
    ImGui::SameLine();
    if (runtime_.mode() == EditorMode::Edit) {
        if (ImGui::Button("Simulate")) {
            runtime_.startSimulate();
        }
        ImGui::SameLine();
        if (ImGui::Button("Play (F5)")) {
            runtime_.startPlayInEditor();
            selectedGuid_ = {};
        }
    } else if (ImGui::Button("Stop (F5)")) {
        runtime_.stop();
        selectedGuid_ = {};
    }

    if (runtime_.mode() == EditorMode::Edit) {
        ImGui::SameLine();
        if (ImGui::Button("Save World")) {
            const bool saved = WorldSerializer::save(
                runtime_.editWorld(),
                std::filesystem::path{ENGINE_CONTENT_DIR} /
                    "Maps" / "Demo.world.json"
            );
            runtime_.log().write(
                saved ? "World saved." : "World save failed."
            );
        }
        ImGui::SameLine();
        if (ImGui::Button("Load World")) {
            auto loaded = WorldSerializer::load(
                std::filesystem::path{ENGINE_CONTENT_DIR} /
                    "Maps" / "Demo.world.json",
                &runtime_.log()
            );
            if (loaded != nullptr) {
                runtime_.setEditWorld(std::move(loaded));
                selectedGuid_ = {};
                runtime_.log().write("World loaded.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Undo")) {
            runtime_.transactions().undo(runtime_.editWorld());
        }
        ImGui::SameLine();
        if (ImGui::Button("Redo")) {
            runtime_.transactions().redo(runtime_.editWorld());
        }
    }
    ImGui::End();
}

void Application::drawWorldOutliner() {
    placeWindow(
        outlinerRect_.x,
        outlinerRect_.y,
        outlinerRect_.width,
        outlinerRect_.height
    );
    ImGui::Begin("World Outliner", nullptr, kFixedPanelFlags);
    for (const auto& actor : runtime_.activeWorld().actors()) {
        ImGui::PushID(actor->guid().toString().c_str());
        const bool open = ImGui::TreeNodeEx(
            actor->name().c_str(),
            ImGuiTreeNodeFlags_DefaultOpen |
                (selectedGuid_ == actor->guid()
                     ? ImGuiTreeNodeFlags_Selected
                     : 0)
        );
        if (ImGui::IsItemClicked()) {
            selectedGuid_ = actor->guid();
        }
        if (open) {
            for (const auto& component : actor->components()) {
                const bool selected = selectedGuid_ == component->guid();
                if (ImGui::Selectable(
                        component->name().c_str(),
                        selected
                    )) {
                    selectedGuid_ = component->guid();
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::End();
}

void Application::drawDetails() {
    placeWindow(
        detailsRect_.x,
        detailsRect_.y,
        detailsRect_.width,
        detailsRect_.height
    );
    ImGui::Begin("Details", nullptr, kFixedPanelFlags);
    Object* object = selectedObject();
    if (object == nullptr || object->typeDescriptor() == nullptr) {
        ImGui::TextUnformatted("Select an Actor or Component.");
        ImGui::End();
        return;
    }

    ImGui::Text("%s (%s)", object->name().c_str(), object->typeName().data());
    std::string currentCategory;
    for (const PropertyDescriptor* property :
         object->typeDescriptor()->allProperties()) {
        if (!hasFlag(property->flags, PropertyFlags::Editable) ||
            !property->getter || !property->setter) {
            continue;
        }
        if (currentCategory != property->category) {
            currentCategory = property->category;
            ImGui::SeparatorText(currentCategory.c_str());
        }

        const PropertyValue before = property->getter(*object);
        PropertyValue after = before;
        bool changed = false;
        ImGui::PushID(property->name.c_str());
        if (auto* booleanValue = std::get_if<bool>(&after)) {
            changed = ImGui::Checkbox(property->name.c_str(), booleanValue);
        } else if (auto* floatValue = std::get_if<float>(&after)) {
            changed = ImGui::DragFloat(
                property->name.c_str(),
                floatValue,
                1.0F
            );
        } else if (auto* integerValue = std::get_if<int>(&after)) {
            changed = ImGui::DragInt(property->name.c_str(), integerValue);
        } else if (auto* vectorValue = std::get_if<glm::vec3>(&after)) {
            changed = ImGui::DragFloat3(
                property->name.c_str(),
                &vectorValue->x,
                1.0F
            );
        } else if (auto* stringValue = std::get_if<std::string>(&after)) {
            std::array<char, 256> buffer{};
            const std::size_t length =
                std::min(stringValue->size(), buffer.size() - 1);
            std::copy_n(stringValue->data(), length, buffer.data());
            if (ImGui::InputText(
                    property->name.c_str(),
                    buffer.data(),
                    buffer.size()
                )) {
                *stringValue = buffer.data();
                changed = true;
            }
        }
        ImGui::PopID();

        if (changed && property->setter(*object, after) &&
            runtime_.mode() == EditorMode::Edit) {
            runtime_.transactions().record(
                object->guid(),
                property->name,
                before,
                after
            );
        }
    }
    ImGui::End();
}

void Application::drawContentBrowser() {
    placeWindow(
        contentRect_.x,
        contentRect_.y,
        contentRect_.width,
        contentRect_.height
    );
    ImGui::Begin("Content Browser", nullptr, kFixedPanelFlags);
    for (const AssetData& asset : runtime_.assets().assets()) {
        ImGui::BulletText(
            "%s [%s]",
            asset.name.c_str(),
            asset.type.c_str()
        );
    }
    if (runtime_.assets().assets().empty()) {
        ImGui::TextUnformatted("No .meta assets found.");
    }
    ImGui::End();
}

void Application::drawOutputLog() {
    placeWindow(
        outputRect_.x,
        outputRect_.y,
        outputRect_.width,
        outputRect_.height
    );
    ImGui::Begin("Output Log", nullptr, kFixedPanelFlags);
    for (const std::string& message : runtime_.log().messages()) {
        ImGui::TextUnformatted(message.c_str());
    }
    ImGui::End();
}

void Application::drawDebugPanel() {
    placeWindow(
        debugRect_.x,
        debugRect_.y,
        debugRect_.width,
        debugRect_.height
    );
    ImGui::Begin("Engine Debug", nullptr, kFixedPanelFlags);
    const World& world = runtime_.activeWorld();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Actors: %zu", world.actors().size());
    ImGui::Text(
        "Render proxies: %zu",
        world.renderScene().proxies().size()
    );
    ImGui::Text(
        "Proxy updates this frame: %zu",
        world.renderScene().updatesLastSync()
    );
    ImGui::Text(
        "Overlap events: %zu",
        world.collision().overlapEvents().size()
    );
    for (const auto& actor : world.actors()) {
        if (const auto* character = dynamic_cast<const Character*>(actor.get())) {
            const glm::vec3 location = character->actorTransform().location;
            ImGui::Text(
                "Player cm: (%.1f, %.1f, %.1f)",
                location.x,
                location.y,
                location.z
            );
            break;
        }
    }
    ImGui::TextUnformatted("WASD: move | F5: Play/Stop | Esc: quit");
    ImGui::End();
}

void Application::drawTransformGizmo(const CameraView& camera) {
    if (runtime_.mode() != EditorMode::Edit) {
        return;
    }

    Object* object = selectedObject();
    SceneComponent* component = dynamic_cast<SceneComponent*>(object);
    if (component == nullptr) {
        if (auto* actor = dynamic_cast<Actor*>(object)) {
            component = actor->rootComponent();
        }
    }
    if (component == nullptr) {
        return;
    }

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(
        viewportRect_.x,
        viewportRect_.y,
        viewportRect_.width,
        viewportRect_.height
    );

    glm::mat4 worldMatrix = component->worldMatrix();
    const glm::vec3 before = component->relativeTransform().location;
    ImGuizmo::Manipulate(
        glm::value_ptr(camera.view),
        glm::value_ptr(camera.projection),
        ImGuizmo::TRANSLATE,
        ImGuizmo::WORLD,
        glm::value_ptr(worldMatrix)
    );
    if (!ImGuizmo::IsUsing()) {
        return;
    }

    const glm::mat4 localMatrix = component->parent() == nullptr
        ? worldMatrix
        : glm::inverse(component->parent()->worldMatrix()) * worldMatrix;
    const Transform local = Transform::fromMatrix(localMatrix);
    component->setRelativeTransform(local);
    if (before != local.location) {
        runtime_.transactions().record(
            component->guid(),
            "Location",
            before,
            local.location
        );
    }
}

CameraView Application::activeCamera(float aspectRatio) const {
    if (runtime_.mode() == EditorMode::Edit) {
        glm::vec3 target{0.0F, 0.0F, 40.0F};
        for (const auto& actor : runtime_.activeWorld().actors()) {
            if (dynamic_cast<const Character*>(actor.get()) != nullptr) {
                target = actor->actorTransform().location;
                break;
            }
        }

        CameraView editorCamera;
        editorCamera.view = glm::lookAt(
            target + glm::vec3{-900.0F, -900.0F, 1050.0F},
            target,
            glm::vec3{0.0F, 0.0F, 1.0F}
        );
        editorCamera.projection = glm::perspective(
            glm::radians(50.0F),
            std::max(aspectRatio, 0.01F),
            10.0F,
            100000.0F
        );
        return editorCamera;
    }

    for (CameraComponent* camera :
         runtime_.activeWorld().componentsOfType<CameraComponent>()) {
        if (camera->active()) {
            return camera->cameraView(aspectRatio);
        }
    }

    CameraView fallback;
    fallback.view = glm::lookAt(
        glm::vec3{-900.0F, -900.0F, 900.0F},
        glm::vec3{0.0F},
        glm::vec3{0.0F, 0.0F, 1.0F}
    );
    fallback.projection = glm::perspective(
        glm::radians(55.0F),
        aspectRatio,
        10.0F,
        100000.0F
    );
    return fallback;
}

Object* Application::selectedObject() {
    if (!selectedGuid_.valid()) {
        return nullptr;
    }
    return runtime_.activeWorld().findObject(selectedGuid_);
}

} // namespace engine
