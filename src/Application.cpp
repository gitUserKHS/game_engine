#include "engine/Application.hpp"

#include "engine/Renderer.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>

#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifndef ENGINE_CONTENT_DIR
#define ENGINE_CONTENT_DIR L"Content"
#endif

namespace {

constexpr float kFixedDeltaTime = 1.0F / 60.0F;
constexpr float kTranslateSnap = 10.0F;
constexpr float kRotateSnap = 15.0F;
constexpr float kScaleSnap = 0.1F;

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

const char* collisionChannelName(int channel) {
    switch (static_cast<engine::CollisionChannel>(channel)) {
    case engine::CollisionChannel::WorldStatic:
        return "WorldStatic";
    case engine::CollisionChannel::WorldDynamic:
        return "WorldDynamic";
    case engine::CollisionChannel::Pawn:
        return "Pawn";
    case engine::CollisionChannel::Visibility:
        return "Visibility";
    case engine::CollisionChannel::Camera:
        return "Camera";
    case engine::CollisionChannel::Count:
        break;
    }
    return "Unknown";
}

std::string timestampText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream text;
    text << std::put_time(&local, "%Y%m%d-%H%M%S");
    return text.str();
}

std::filesystem::path pathFromUtf8(std::string_view text) {
    std::u8string utf8;
    utf8.reserve(text.size());
    for (const char character : text) {
        utf8.push_back(static_cast<char8_t>(
            static_cast<unsigned char>(character)
        ));
    }
    return std::filesystem::path{utf8};
}

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return {
        reinterpret_cast<const char*>(utf8.data()),
        utf8.size(),
    };
}

std::string extensionLower(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& character : extension) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }
    return extension;
}

std::filesystem::path pngPath(
    const std::filesystem::path& requested,
    std::string_view suffix,
    bool addSuffix
) {
    std::filesystem::path directory = requested.parent_path();
    std::filesystem::path stem = requested.has_extension()
        ? requested.stem()
        : requested.filename();
    if (stem.empty()) {
        stem = timestampText();
    }
    if (addSuffix) {
        stem += pathFromUtf8(suffix);
    }
    stem += L".png";
    return directory / stem;
}

glm::vec3 matrixScale(const glm::mat4& matrix) {
    return {
        glm::length(glm::vec3{matrix[0]}),
        glm::length(glm::vec3{matrix[1]}),
        glm::length(glm::vec3{matrix[2]}),
    };
}

} // namespace

namespace engine {

Application::Application(ApplicationOptions options)
    : options_(std::move(options)) {
    registerEngineTypes();
    createDemoWorld();
    runtime_.assets().scan(ENGINE_CONTENT_DIR, &runtime_.log());
    runtime_.input().loadConfig(
        std::filesystem::path{ENGINE_CONTENT_DIR} /
            "Input" / "default.input.json",
        &runtime_.log()
    );
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
        frameDeltaTime_ = static_cast<float>(
            std::min(currentTime - previousTime, 0.25)
        );
        accumulator += frameDeltaTime_;
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
    return exitCode_;
}

bool Application::initialize() {
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "GLFW initialization failed.\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, options_.hidden ? GLFW_FALSE : GLFW_TRUE);
    window_ = glfwCreateWindow(
        options_.windowWidth,
        options_.windowHeight,
        "Cocoa Engine - Editable Unreal-inspired learning engine",
        nullptr,
        nullptr
    );
    if (window_ == nullptr) {
        std::cerr << "Window creation failed.\n";
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(options_.hidden ? 0 : 1);
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        std::cerr << "GLAD could not load OpenGL functions.\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    renderer_ = std::make_unique<Renderer>();
    viewportTarget_ = std::make_unique<ViewportRenderTarget>();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    std::filesystem::create_directories(
        std::filesystem::path{"Saved"} / "Editor"
    );
    imguiIniPath_ = (
        std::filesystem::path{"Saved"} / "Editor" / "imgui.ini"
    ).string();
    resetLayout_ = !std::filesystem::exists(imguiIniPath_);
    io.IniFilename = imguiIniPath_.c_str();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 2.0F;
    style.ChildRounding = 3.0F;
    style.FrameRounding = 3.0F;
    style.WindowBorderSize = 1.0F;
    style.WindowPadding = {8.0F, 7.0F};
    style.ItemSpacing = {7.0F, 5.0F};
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    return true;
}

void Application::shutdown() {
    runtime_.stop();
    if (window_ != nullptr) {
        glfwMakeContextCurrent(window_);
    }
    viewportTarget_.reset();
    renderer_.reset();
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
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
        ImGui::GetCurrentContext() != nullptr &&
        ImGui::GetIO().WantCaptureKeyboard;
    const bool editorCameraOwnsKeyboard =
        viewportLookActive_ || viewportOrbitActive_ || viewportPanActive_;
    const auto set = [this, capture, editorCameraOwnsKeyboard](
                         Key key,
                         int glfwKey
                     ) {
        runtime_.input().setKeyDown(
            key,
            !capture && !editorCameraOwnsKeyboard &&
                glfwGetKey(window_, glfwKey) == GLFW_PRESS
        );
    };
    set(Key::W, GLFW_KEY_W);
    set(Key::A, GLFW_KEY_A);
    set(Key::S, GLFW_KEY_S);
    set(Key::D, GLFW_KEY_D);
    set(Key::Q, GLFW_KEY_Q);
    set(Key::E, GLFW_KEY_E);
    set(Key::Space, GLFW_KEY_SPACE);
    set(Key::Escape, GLFW_KEY_ESCAPE);
    set(Key::F5, GLFW_KEY_F5);

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

    const bool f9Down = glfwGetKey(window_, GLFW_KEY_F9) == GLFW_PRESS;
    if (!capture && f9Down && !f9WasDown_) {
        queueManualScreenshot();
    }
    f9WasDown_ = f9Down;
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
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawDockspace();
    drawToolbar();
    drawViewportPanel();
    drawWorldOutliner();
    drawDetails();
    drawContentBrowser();
    drawOutputLog();
    drawDebugPanel();

    renderer_->clearBackbuffer(framebufferWidth, framebufferHeight);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ++renderedFrames_;
    if (options_.screenshot.has_value() && !commandLineCaptureQueued_ &&
        renderedFrames_ >=
            static_cast<std::uint64_t>(options_.screenshot->frame)) {
        pendingScreenshot_ = *options_.screenshot;
        commandLineCaptureQueued_ = true;
    }
    if (pendingScreenshot_.has_value()) {
        const ScreenshotRequest request = *pendingScreenshot_;
        pendingScreenshot_.reset();
        processScreenshot(request, framebufferWidth, framebufferHeight);
    }
}

void Application::drawDockspace() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("Editor DockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceId = ImGui::GetID("CocoaEditorDockspace");
    ImGui::DockSpace(
        dockspaceId,
        {0.0F, 0.0F},
        ImGuiDockNodeFlags_PassthruCentralNode
    );
    if (resetLayout_) {
        buildDefaultDockLayout();
        resetLayout_ = false;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Screenshot", "F9")) {
                queueManualScreenshot();
            }
            if (ImGui::MenuItem("Exit", "Esc")) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Reset Layout")) {
                resetLayout_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::End();
}

void Application::buildDefaultDockLayout() {
    const ImGuiID dockspaceId = ImGui::GetID("CocoaEditorDockspace");
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(
        dockspaceId,
        ImGuiDockNodeFlags_DockSpace
    );
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID toolbar = 0;
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGuiID bottom = 0;
    ImGui::DockBuilderSplitNode(
        center,
        ImGuiDir_Up,
        0.075F,
        &toolbar,
        &center
    );
    ImGui::DockBuilderSplitNode(
        center,
        ImGuiDir_Left,
        0.18F,
        &left,
        &center
    );
    ImGui::DockBuilderSplitNode(
        center,
        ImGuiDir_Right,
        0.24F,
        &right,
        &center
    );
    ImGui::DockBuilderSplitNode(
        center,
        ImGuiDir_Down,
        0.25F,
        &bottom,
        &center
    );

    ImGuiID content = 0;
    ImGuiID outputAndDebug = bottom;
    ImGui::DockBuilderSplitNode(
        outputAndDebug,
        ImGuiDir_Left,
        0.30F,
        &content,
        &outputAndDebug
    );
    ImGuiID debug = 0;
    ImGuiID output = outputAndDebug;
    ImGui::DockBuilderSplitNode(
        output,
        ImGuiDir_Right,
        0.32F,
        &debug,
        &output
    );

    ImGui::DockBuilderDockWindow("Toolbar", toolbar);
    ImGui::DockBuilderDockWindow("World Outliner", left);
    ImGui::DockBuilderDockWindow("Details", right);
    ImGui::DockBuilderDockWindow("Viewport", center);
    ImGui::DockBuilderDockWindow("Content Browser", content);
    ImGui::DockBuilderDockWindow("Output Log", output);
    ImGui::DockBuilderDockWindow("Engine Debug", debug);
    ImGui::DockBuilderFinish(dockspaceId);
}

void Application::drawViewportPanel() {
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::Begin("Viewport", nullptr, flags);
    ImGui::PopStyleVar();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const int width = std::max(static_cast<int>(available.x), 1);
    const int height = std::max(static_cast<int>(available.y), 1);
    if (!viewportTarget_->resize(width, height)) {
        runtime_.log().write("Viewport framebuffer is incomplete.");
    }

    const ImVec2 imageStart = ImGui::GetCursorScreenPos();
    ImGui::Image(
        reinterpret_cast<ImTextureID>(
            static_cast<std::intptr_t>(viewportTarget_->colorTexture())
        ),
        {static_cast<float>(width), static_cast<float>(height)},
        {0.0F, 1.0F},
        {1.0F, 0.0F}
    );
    viewportRect_ = {
        imageStart.x,
        imageStart.y,
        static_cast<float>(width),
        static_cast<float>(height),
    };
    const bool hovered = ImGui::IsItemHovered();

    updateEditorCamera(hovered);
    const float aspect =
        static_cast<float>(width) / static_cast<float>(height);
    const CameraView camera = activeCamera(aspect);
    World& world = runtime_.activeWorld();
    world.renderScene().sync(world);
    const std::vector<Guid> selectedComponents = selectedRenderComponents();
    renderer_->renderToTarget(
        *viewportTarget_,
        world.renderScene(),
        camera,
        selectedComponents
    );

    drawTransformGizmo(camera);
    if (runtime_.mode() == EditorMode::Edit && hovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::GetIO().KeyAlt && !ImGuizmo::IsOver() &&
        !ImGuizmo::IsUsing()) {
        selectFromViewport(camera);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(
        {imageStart.x + 10.0F, imageStart.y + 9.0F},
        IM_COL32(210, 218, 230, 230),
        (
            std::string{"Viewport | "} + modeName(runtime_.mode()) +
            " | RMB+WASD/QE | Alt+LMB Orbit | F Focus"
        ).c_str()
    );
    ImGui::End();
}

void Application::drawToolbar() {
    ImGui::Begin(
        "Toolbar",
        nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
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

    const bool editing = runtime_.mode() == EditorMode::Edit;
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Add")) {
        ImGui::OpenPopup("AddActorPopup");
    }
    if (ImGui::BeginPopup("AddActorPopup")) {
        if (ImGui::MenuItem("Empty Actor")) {
            createEmptyActor();
        }
        if (ImGui::MenuItem("Cube Actor")) {
            createCubeActor();
        }
        ImGui::EndMenu();
    }
    ImGui::SameLine();
    if (ImGui::Button("W Move")) {
        transformTool_ = TransformTool::Translate;
    }
    ImGui::SameLine();
    if (ImGui::Button("E Rotate")) {
        transformTool_ = TransformTool::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::Button("R Scale")) {
        transformTool_ = TransformTool::Scale;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Local", &localTransform_);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snapping_);
    ImGui::SameLine();
    if (ImGui::Button("Undo")) {
        undo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        redo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Screenshot (F9)")) {
        queueManualScreenshot();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save World") && editing) {
        const bool saved = WorldSerializer::save(
            runtime_.editWorld(),
            std::filesystem::path{ENGINE_CONTENT_DIR} /
                "Maps" / "Demo.world.json"
        );
        runtime_.log().write(saved ? "World saved." : "World save failed.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load World") && editing) {
        auto loaded = WorldSerializer::load(
            std::filesystem::path{ENGINE_CONTENT_DIR} /
                "Maps" / "Demo.world.json",
            &runtime_.log()
        );
        if (loaded != nullptr) {
            runtime_.setEditWorld(std::move(loaded));
            selectedGuid_ = {};
            runtime_.transactions().clear();
            runtime_.log().write("World loaded.");
        }
    }
    ImGui::End();
}

void Application::drawWorldOutliner() {
    ImGui::Begin("World Outliner");
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
                if (ImGui::Selectable(component->name().c_str(), selected)) {
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
    ImGui::Begin("Details");
    Object* object = selectedObject();
    if (object == nullptr || object->typeDescriptor() == nullptr) {
        ImGui::TextUnformatted("Select an Actor or Component.");
        ImGui::End();
        return;
    }

    ImGui::Text("%s (%s)", object->name().c_str(), object->typeName().data());
    ImGui::BeginDisabled(runtime_.mode() != EditorMode::Edit);
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
            if (property->name == "ObjectChannel") {
                int current = std::clamp(
                    *integerValue,
                    0,
                    static_cast<int>(CollisionChannel::Count) - 1
                );
                if (ImGui::BeginCombo(
                        property->name.c_str(),
                        collisionChannelName(current)
                    )) {
                    for (int channel = 0;
                         channel < static_cast<int>(CollisionChannel::Count);
                         ++channel) {
                        const bool selected = current == channel;
                        if (ImGui::Selectable(
                                collisionChannelName(channel),
                                selected
                            )) {
                            current = channel;
                            *integerValue = channel;
                            changed = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                changed = ImGui::DragInt(property->name.c_str(), integerValue);
            }
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

        const bool activated = ImGui::IsItemActivated();
        if (activated && runtime_.mode() == EditorMode::Edit) {
            pendingPropertyEdit_ = PendingPropertyEdit{
                object->guid(),
                property->name,
                before,
            };
        }
        if (changed) {
            property->setter(*object, after);
        }
        if (ImGui::IsItemDeactivatedAfterEdit() &&
            runtime_.mode() == EditorMode::Edit &&
            pendingPropertyEdit_.has_value() &&
            pendingPropertyEdit_->object == object->guid() &&
            pendingPropertyEdit_->property == property->name) {
            runtime_.transactions().record(
                object->guid(),
                property->name,
                pendingPropertyEdit_->before,
                property->getter(*object)
            );
            pendingPropertyEdit_.reset();
        } else if (changed && !activated &&
                   runtime_.mode() == EditorMode::Edit &&
                   !pendingPropertyEdit_.has_value()) {
            runtime_.transactions().record(
                object->guid(),
                property->name,
                before,
                property->getter(*object)
            );
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

void Application::drawContentBrowser() {
    ImGui::Begin("Content Browser");
    const bool editing = runtime_.mode() == EditorMode::Edit;
    ImGui::TextUnformatted("Import external .gltf/.glb or image files.");
    ImGui::InputText(
        "Source Path",
        importPathBuffer_.data(),
        importPathBuffer_.size()
    );
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Import")) {
        importAssetFromPath(importPathBuffer_.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan Content")) {
        rescanAssets();
    }
    ImGui::EndDisabled();
    if (!editing) {
        ImGui::TextUnformatted("Import is available in Edit mode.");
    }
    ImGui::Separator();
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
    ImGui::Begin("Output Log");
    for (const std::string& message : runtime_.log().messages()) {
        ImGui::TextUnformatted(message.c_str());
    }
    ImGui::End();
}

void Application::drawDebugPanel() {
    ImGui::Begin("Engine Debug");
    const World& world = runtime_.activeWorld();
    const RenderScene& scene = world.renderScene();
    const std::vector<Guid> selectedComponents = selectedRenderComponents();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Actors: %zu", world.actors().size());
    ImGui::Text("Render proxies: %zu", scene.proxies().size());
    ImGui::Text("Opaque proxies: %zu", scene.opaqueProxyCount());
    ImGui::Text("Debug wire proxies: %zu", scene.debugWireProxyCount());
    ImGui::Text("Directional lights: %zu", scene.lights().size());
    ImGui::Text("Proxy updates: %zu", scene.updatesLastSync());
    ImGui::SeparatorText("Render Passes");
    ImGui::TextDisabled("Shadow: planned");
    ImGui::Text("Opaque: grid + %zu lit mesh proxies", scene.opaqueProxyCount());
    ImGui::Text(
        "Debug: %zu wire proxies + %zu selection overlays",
        scene.debugWireProxyCount(),
        selectedComponents.size()
    );
    ImGui::TextUnformatted("UI: Dear ImGui dockspace and panels");
    ImGui::Separator();
    ImGui::Text(
        "Camera cm: %.1f, %.1f, %.1f",
        editorViewport_.position().x,
        editorViewport_.position().y,
        editorViewport_.position().z
    );
    ImGui::Text(
        "Yaw/Pitch: %.1f / %.1f",
        editorViewport_.yawDegrees(),
        editorViewport_.pitchDegrees()
    );
    ImGui::Separator();
    ImGui::TextUnformatted("RMB+WASD/QE: fly camera");
    ImGui::TextUnformatted("Alt+LMB: orbit | MMB: pan | Wheel: dolly");
    ImGui::TextUnformatted("LMB: select | F: focus | W/E/R: gizmo");
    ImGui::TextUnformatted("Ctrl+D: duplicate | Delete: remove | F9: capture");
    ImGui::End();
}

void Application::drawTransformGizmo(const CameraView& camera) {
    if (runtime_.mode() != EditorMode::Edit) {
        gizmoUsing_ = false;
        return;
    }

    SceneComponent* component = selectedSceneComponent();
    if (component == nullptr) {
        gizmoUsing_ = false;
        return;
    }

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(
        viewportRect_.x,
        viewportRect_.y,
        viewportRect_.width,
        viewportRect_.height
    );

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    float snap[3]{kTranslateSnap, kTranslateSnap, kTranslateSnap};
    if (transformTool_ == TransformTool::Rotate) {
        operation = ImGuizmo::ROTATE;
        snap[0] = snap[1] = snap[2] = kRotateSnap;
    } else if (transformTool_ == TransformTool::Scale) {
        operation = ImGuizmo::SCALE;
        snap[0] = snap[1] = snap[2] = kScaleSnap;
    }

    glm::mat4 worldMatrix = component->worldMatrix();
    ImGuizmo::Manipulate(
        glm::value_ptr(camera.view),
        glm::value_ptr(camera.projection),
        operation,
        localTransform_ ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
        glm::value_ptr(worldMatrix),
        nullptr,
        snapping_ ? snap : nullptr
    );

    const bool usingNow = ImGuizmo::IsUsing();
    if (usingNow && !gizmoUsing_) {
        gizmoBefore_ = component->relativeTransform();
    }
    if (usingNow) {
        const glm::mat4 localMatrix = component->parent() == nullptr
            ? worldMatrix
            : glm::inverse(component->parent()->worldMatrix()) * worldMatrix;
        component->setRelativeTransform(Transform::fromMatrix(localMatrix));
    } else if (gizmoUsing_) {
        runtime_.transactions().recordTransform(
            component->guid(),
            gizmoBefore_,
            component->relativeTransform()
        );
    }
    gizmoUsing_ = usingNow;
}

void Application::updateEditorCamera(bool viewportHovered) {
    if (runtime_.mode() == EditorMode::PlayInEditor) {
        viewportLookActive_ = false;
        viewportOrbitActive_ = false;
        viewportPanActive_ = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (viewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        viewportLookActive_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        viewportLookActive_ = false;
    }
    if (viewportHovered && io.KeyAlt &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (const Actor* actor = selectedActor()) {
            editorViewport_.setPivot(actor->actorTransform().location);
        }
        viewportOrbitActive_ = true;
    }
    if (!io.KeyAlt || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        viewportOrbitActive_ = false;
    }
    if (viewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        viewportPanActive_ = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        viewportPanActive_ = false;
    }

    const bool cameraActive =
        viewportLookActive_ || viewportOrbitActive_ || viewportPanActive_;
    EditorViewportInput input;
    input.deltaTime = frameDeltaTime_;
    input.mouseDelta = {io.MouseDelta.x, io.MouseDelta.y};
    input.mouseWheel = viewportHovered ? io.MouseWheel : 0.0F;
    input.look = viewportLookActive_;
    input.orbit = viewportOrbitActive_;
    input.pan = viewportPanActive_;
    input.fast = io.KeyShift;
    if (viewportLookActive_) {
        input.moveForward = ImGui::IsKeyDown(ImGuiKey_W);
        input.moveBackward = ImGui::IsKeyDown(ImGuiKey_S);
        input.moveRight = ImGui::IsKeyDown(ImGuiKey_D);
        input.moveLeft = ImGui::IsKeyDown(ImGuiKey_A);
        input.moveUp = ImGui::IsKeyDown(ImGuiKey_E);
        input.moveDown = ImGui::IsKeyDown(ImGuiKey_Q);
    }
    editorViewport_.update(input);
    if (cameraActive) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    if (viewportHovered && !cameraActive &&
        !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            focusSelection();
        }
        if (runtime_.mode() == EditorMode::Edit) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                transformTool_ = TransformTool::Translate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                transformTool_ = TransformTool::Rotate;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                transformTool_ = TransformTool::Scale;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                duplicateSelection();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                deleteSelection();
            }
        }
    }
}

void Application::selectFromViewport(const CameraView&) {
    const ImVec2 mouse = ImGui::GetMousePos();
    const glm::vec2 local{
        mouse.x - viewportRect_.x,
        mouse.y - viewportRect_.y,
    };
    const EditorRay ray = editorViewport_.screenRay(
        local,
        {viewportRect_.width, viewportRect_.height}
    );
    const auto hit = pickRenderProxy(
        runtime_.activeWorld().renderScene(),
        ray
    );
    if (!hit.has_value()) {
        selectedGuid_ = {};
        return;
    }

    Object* object =
        runtime_.activeWorld().findObject(hit->componentGuid);
    if (auto* component = dynamic_cast<ActorComponent*>(object)) {
        selectedGuid_ = component->owner()->guid();
    } else if (auto* actor = dynamic_cast<Actor*>(object)) {
        selectedGuid_ = actor->guid();
    }
}

void Application::focusSelection() {
    Actor* actor = selectedActor();
    if (actor == nullptr) {
        return;
    }

    bool found = false;
    glm::vec3 center{0.0F};
    float radius = 100.0F;
    for (const RenderProxy& proxy :
         runtime_.activeWorld().renderScene().proxies()) {
        Object* object =
            runtime_.activeWorld().findObject(proxy.componentGuid);
        auto* component = dynamic_cast<ActorComponent*>(object);
        if (component == nullptr || component->owner() != actor) {
            continue;
        }
        const glm::vec3 proxyCenter = glm::vec3{proxy.worldMatrix[3]};
        const float proxyRadius =
            glm::length(matrixScale(proxy.worldMatrix)) * 0.5F;
        if (!found) {
            center = proxyCenter;
            radius = proxyRadius;
            found = true;
        } else {
            radius = std::max(
                radius,
                glm::distance(center, proxyCenter) + proxyRadius
            );
        }
    }
    if (!found) {
        center = actor->actorTransform().location;
    }
    editorViewport_.focus(center, radius);
}

void Application::createEmptyActor() {
    World& world = runtime_.editWorld();
    const std::string before = WorldSerializer::toJson(world);
    Actor& actor = world.spawnActor<Actor>(uniqueActorName("Actor"));
    auto& root = actor.addComponent<SceneComponent>("Root");
    root.setRelativeLocation(editorViewport_.pivot());
    actor.setRootComponent(&root);
    selectedGuid_ = actor.guid();
    runtime_.transactions().recordSnapshot(
        before,
        WorldSerializer::toJson(world)
    );
}

void Application::createCubeActor() {
    World& world = runtime_.editWorld();
    const std::string before = WorldSerializer::toJson(world);
    Actor& actor = world.spawnActor<Actor>(uniqueActorName("Cube"));
    auto& collision = actor.addComponent<BoxComponent>("Collision");
    collision.setRelativeLocation(editorViewport_.pivot());
    collision.setExtent({50.0F, 50.0F, 50.0F});
    collision.setObjectChannel(CollisionChannel::WorldStatic);
    collision.setDrawDebug(true);
    actor.setRootComponent(&collision);

    auto& mesh = actor.addComponent<StaticMeshComponent>("Mesh");
    mesh.attachTo(&collision);
    Transform meshTransform;
    meshTransform.scale = {100.0F, 100.0F, 100.0F};
    mesh.setRelativeTransform(meshTransform);
    MaterialInstance material;
    material.baseColor = {0.22F, 0.58F, 0.92F};
    mesh.setMaterial(material);

    selectedGuid_ = actor.guid();
    runtime_.transactions().recordSnapshot(
        before,
        WorldSerializer::toJson(world)
    );
}

void Application::duplicateSelection() {
    Actor* source = selectedActor();
    if (source == nullptr) {
        return;
    }
    World& world = runtime_.editWorld();
    const std::string before = WorldSerializer::toJson(world);
    Actor* duplicate = WorldSerializer::duplicateActor(
        world,
        *source,
        uniqueActorName(source->name()),
        &runtime_.log()
    );
    if (duplicate == nullptr) {
        runtime_.log().write("Actor duplication failed.");
        return;
    }
    if (duplicate->rootComponent() != nullptr) {
        Transform transform = duplicate->rootComponent()->relativeTransform();
        transform.location += glm::vec3{50.0F, 50.0F, 0.0F};
        duplicate->rootComponent()->setRelativeTransform(transform);
    }
    selectedGuid_ = duplicate->guid();
    runtime_.transactions().recordSnapshot(
        before,
        WorldSerializer::toJson(world)
    );
}

void Application::deleteSelection() {
    Actor* actor = selectedActor();
    if (actor == nullptr) {
        return;
    }
    World& world = runtime_.editWorld();
    const std::string before = WorldSerializer::toJson(world);
    world.destroyActor(*actor);
    selectedGuid_ = {};
    runtime_.transactions().recordSnapshot(
        before,
        WorldSerializer::toJson(world)
    );
}

void Application::undo() {
    if (runtime_.transactions().undo(runtime_.editWorld())) {
        if (selectedObject() == nullptr) {
            selectedGuid_ = {};
        }
        pendingPropertyEdit_.reset();
    }
}

void Application::redo() {
    if (runtime_.transactions().redo(runtime_.editWorld())) {
        if (selectedObject() == nullptr) {
            selectedGuid_ = {};
        }
        pendingPropertyEdit_.reset();
    }
}

void Application::importAssetFromPath(std::string_view pathText) {
    const std::string text{pathText};
    if (text.empty()) {
        runtime_.log().write("Import path is empty.");
        return;
    }

    const std::filesystem::path source = pathFromUtf8(text);
    const std::string extension = extensionLower(source);
    AssetImportResult result;
    if (extension == ".gltf" || extension == ".glb") {
        result = AssetImporter::importGltfAsStaticMesh(
            source,
            ENGINE_CONTENT_DIR,
            &runtime_.log()
        );
    } else if (
        extension == ".png" || extension == ".jpg" ||
        extension == ".jpeg" || extension == ".bmp" ||
        extension == ".tga"
    ) {
        result = AssetImporter::importTexture(
            source,
            ENGINE_CONTENT_DIR,
            &runtime_.log()
        );
    } else {
        runtime_.log().write(
            "Import supports .gltf, .glb, .png, .jpg, .jpeg, .bmp, and .tga."
        );
        return;
    }

    if (!result.success) {
        runtime_.log().write("Import failed: " + text);
        return;
    }

    rescanAssets();
    runtime_.log().write(
        "Imported " + result.asset.name + " as " + result.asset.type + "."
    );
}

void Application::rescanAssets() {
    runtime_.assets().scan(ENGINE_CONTENT_DIR, &runtime_.log());
    runtime_.log().write("Content assets rescanned.");
}

void Application::queueManualScreenshot() {
    pendingScreenshot_ = ScreenshotRequest{
        std::filesystem::path{"Saved"} / "Screenshots" / timestampText(),
        ScreenshotTarget::Both,
        1,
        false,
    };
}

void Application::processScreenshot(
    const ScreenshotRequest& request,
    int framebufferWidth,
    int framebufferHeight
) {
    const bool both = request.target == ScreenshotTarget::Both;
    ScreenshotResult result;
    result.success = true;

    const auto writeCapture = [&](
                                  std::string_view suffix,
                                  int width,
                                  int height,
                                  std::vector<unsigned char> pixels
                              ) {
        const std::filesystem::path image =
            pngPath(request.path, suffix, both);
        std::string error;
        if (!writePngRgba(
                image,
                width,
                height,
                pixels,
                true,
                &error
            )) {
            result.success = false;
            result.error += error + " ";
            return;
        }

        nlohmann::json metadata{
            {"world", runtime_.activeWorld().name()},
            {"mode", modeName(runtime_.mode())},
            {"target",
             suffix.empty() ? "capture" : std::string{suffix.substr(1)}},
            {"width", width},
            {"height", height},
            {"actorCount", runtime_.activeWorld().actors().size()},
            {"selectedGuid",
             selectedGuid_.valid() ? selectedGuid_.toString() : ""},
            {"selectedName",
             selectedObject() == nullptr ? "" : selectedObject()->name()},
            {"camera",
             {
                 {"position",
                  {
                      editorViewport_.position().x,
                      editorViewport_.position().y,
                      editorViewport_.position().z,
                  }},
                 {"yawDegrees", editorViewport_.yawDegrees()},
                 {"pitchDegrees", editorViewport_.pitchDegrees()},
             }},
        };
        std::filesystem::path metadataPath = image;
        metadataPath.replace_extension(".json");
        std::ofstream stream(metadataPath);
        if (!stream) {
            result.success = false;
            result.error += "Could not write screenshot metadata. ";
            return;
        }
        stream << metadata.dump(2);
        result.images.push_back(image);
        result.metadata.push_back(metadataPath);
    };

    if (request.target == ScreenshotTarget::Editor || both) {
        writeCapture(
            "-editor",
            framebufferWidth,
            framebufferHeight,
            renderer_->readBackbufferRgba(
                framebufferWidth,
                framebufferHeight
            )
        );
    }
    if (request.target == ScreenshotTarget::Viewport || both) {
        writeCapture(
            "-viewport",
            viewportTarget_->width(),
            viewportTarget_->height(),
            viewportTarget_->readRgba()
        );
    }

    if (result.success) {
        for (const auto& image : result.images) {
            runtime_.log().write(
                "Screenshot saved: " + pathToUtf8(image)
            );
        }
    } else {
        runtime_.log().write("Screenshot failed: " + result.error);
        std::cerr << "Screenshot failed: " << result.error << '\n';
        exitCode_ = 2;
    }
    if (request.exitAfterCapture) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

CameraView Application::activeCamera(float aspectRatio) const {
    if (runtime_.mode() != EditorMode::PlayInEditor) {
        return editorViewport_.cameraView(aspectRatio);
    }

    for (CameraComponent* camera :
         runtime_.activeWorld().componentsOfType<CameraComponent>()) {
        if (camera->active()) {
            return camera->cameraView(aspectRatio);
        }
    }
    return editorViewport_.cameraView(aspectRatio);
}

Object* Application::selectedObject() {
    if (!selectedGuid_.valid()) {
        return nullptr;
    }
    return runtime_.activeWorld().findObject(selectedGuid_);
}

Actor* Application::selectedActor() {
    Object* object = selectedObject();
    if (auto* actor = dynamic_cast<Actor*>(object)) {
        return actor;
    }
    if (auto* component = dynamic_cast<ActorComponent*>(object)) {
        return component->owner();
    }
    return nullptr;
}

SceneComponent* Application::selectedSceneComponent() {
    Object* object = selectedObject();
    if (auto* component = dynamic_cast<SceneComponent*>(object)) {
        return component;
    }
    if (auto* actor = dynamic_cast<Actor*>(object)) {
        return actor->rootComponent();
    }
    return nullptr;
}

std::vector<Guid> Application::selectedRenderComponents() const {
    std::vector<Guid> result;
    if (!selectedGuid_.valid()) {
        return result;
    }

    const World& world = runtime_.activeWorld();
    Object* selected = world.findObject(selectedGuid_);
    const Actor* actor = dynamic_cast<const Actor*>(selected);
    if (actor == nullptr) {
        if (const auto* component =
                dynamic_cast<const ActorComponent*>(selected)) {
            actor = component->owner();
        }
    }
    if (actor == nullptr) {
        return result;
    }

    for (const RenderProxy& proxy : world.renderScene().proxies()) {
        Object* object = world.findObject(proxy.componentGuid);
        const auto* component = dynamic_cast<const ActorComponent*>(object);
        if (component != nullptr && component->owner() == actor) {
            result.push_back(proxy.componentGuid);
        }
    }
    return result;
}

std::string Application::uniqueActorName(std::string_view base) const {
    const auto exists = [this](const std::string& candidate) {
        return std::any_of(
            runtime_.editWorld().actors().begin(),
            runtime_.editWorld().actors().end(),
            [&candidate](const auto& actor) {
                return actor->name() == candidate;
            }
        );
    };
    const std::string first{base};
    if (!exists(first)) {
        return first;
    }
    for (int suffix = 2;; ++suffix) {
        const std::string candidate =
            first + " " + std::to_string(suffix);
        if (!exists(candidate)) {
            return candidate;
        }
    }
}

} // namespace engine
