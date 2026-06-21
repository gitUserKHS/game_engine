#pragma once

#include "engine/Editor.hpp"
#include "engine/Gameplay.hpp"

#include <memory>
#include <optional>
#include <string>

struct GLFWwindow;

namespace engine {

class Renderer;
class ViewportRenderTarget;

/// GLFW/OpenGL 초기화, 고정 timestep 루프, 도킹 에디터를 묶는 프로그램 셸이다.
class Application {
public:
    explicit Application(ApplicationOptions options = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    enum class TransformTool {
        Translate,
        Rotate,
        Scale,
    };

    struct PanelRect {
        float x{0.0F};
        float y{0.0F};
        float width{0.0F};
        float height{0.0F};
    };

    struct PendingPropertyEdit {
        Guid object;
        std::string property;
        PropertyValue before;
    };

    bool initialize();
    void shutdown();
    void processInput();
    void fixedUpdate(float deltaTime);
    void render();
    void createDemoWorld();

    void drawDockspace();
    void buildDefaultDockLayout();
    void drawViewportPanel();
    void drawToolbar();
    void drawWorldOutliner();
    void drawDetails();
    void drawContentBrowser();
    void drawOutputLog();
    void drawDebugPanel();
    void drawTransformGizmo(const CameraView& camera);

    void updateEditorCamera(bool viewportHovered);
    void selectFromViewport(const CameraView& camera);
    void focusSelection();
    void createEmptyActor();
    void createCubeActor();
    void duplicateSelection();
    void deleteSelection();
    void undo();
    void redo();

    void queueManualScreenshot();
    void processScreenshot(
        const ScreenshotRequest& request,
        int framebufferWidth,
        int framebufferHeight
    );

    [[nodiscard]] CameraView activeCamera(float aspectRatio) const;
    [[nodiscard]] Object* selectedObject();
    [[nodiscard]] Actor* selectedActor();
    [[nodiscard]] SceneComponent* selectedSceneComponent();
    [[nodiscard]] std::vector<Guid> selectedRenderComponents() const;
    [[nodiscard]] std::string uniqueActorName(std::string_view base) const;

    ApplicationOptions options_;
    GLFWwindow* window_{nullptr};
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ViewportRenderTarget> viewportTarget_;
    EngineRuntime runtime_;
    EditorViewportController editorViewport_;
    Guid selectedGuid_;
    PanelRect viewportRect_;
    TransformTool transformTool_{TransformTool::Translate};
    bool localTransform_{false};
    bool snapping_{true};
    bool resetLayout_{false};
    bool viewportLookActive_{false};
    bool viewportOrbitActive_{false};
    bool viewportPanActive_{false};
    bool gizmoUsing_{false};
    Transform gizmoBefore_;
    std::optional<PendingPropertyEdit> pendingPropertyEdit_;
    std::optional<ScreenshotRequest> pendingScreenshot_;
    std::string imguiIniPath_;
    float frameDeltaTime_{0.0F};
    std::uint64_t renderedFrames_{0};
    bool commandLineCaptureQueued_{false};
    bool f5WasDown_{false};
    bool f9WasDown_{false};
    int exitCode_{0};
};

} // namespace engine
