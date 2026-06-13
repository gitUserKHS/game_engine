#pragma once

#include "engine/Gameplay.hpp"

#include <memory>

struct GLFWwindow;

namespace engine {

class Renderer;

/// GLFW/OpenGL 초기화, 고정 timestep 루프, 통합 ImGui 에디터를 묶는 프로그램 셸이다.
class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    struct PanelRect {
        float x{0.0F};
        float y{0.0F};
        float width{0.0F};
        float height{0.0F};
    };

    bool initialize();
    void shutdown();
    void processInput();
    void fixedUpdate(float deltaTime);
    void render();
    void createDemoWorld();
    void updateEditorLayout(int width, int height);

    void drawViewportPanel();
    void drawToolbar();
    void drawWorldOutliner();
    void drawDetails();
    void drawContentBrowser();
    void drawOutputLog();
    void drawDebugPanel();
    void drawTransformGizmo(const CameraView& camera);

    [[nodiscard]] CameraView activeCamera(float aspectRatio) const;
    [[nodiscard]] Object* selectedObject();

    GLFWwindow* window_{nullptr};
    std::unique_ptr<Renderer> renderer_;
    EngineRuntime runtime_;
    Guid selectedGuid_;
    PanelRect toolbarRect_;
    PanelRect outlinerRect_;
    PanelRect detailsRect_;
    PanelRect contentRect_;
    PanelRect outputRect_;
    PanelRect debugRect_;
    PanelRect viewportRect_;
    bool f5WasDown_{false};
};

} // namespace engine
