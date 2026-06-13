#pragma once

#include "engine/RenderTypes.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <string>

namespace engine {

class RenderScene;

/// OpenGL 객체와 draw call을 소유하는 유일한 계층이다. GL context보다 먼저 파괴된다.
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void beginFrame(
        const CameraView& camera,
        int framebufferWidth,
        int framebufferHeight,
        int viewportX,
        int viewportY,
        int viewportWidth,
        int viewportHeight
    );
    void render(const RenderScene& scene) const;
    void drawGrid() const;

private:
    static unsigned int compileShader(unsigned int type, const std::string& source);
    static std::string readTextFile(const std::string& path);
    static unsigned int createProgram();
    void createCubeMesh();
    void createGridMesh(float halfExtent, float spacing);
    void drawCubeModel(const glm::mat4& model, const glm::vec3& color) const;

    unsigned int program_{0};
    unsigned int cubeVao_{0};
    unsigned int cubeVbo_{0};
    unsigned int cubeEbo_{0};
    unsigned int gridVao_{0};
    unsigned int gridVbo_{0};
    int gridVertexCount_{0};
    int mvpLocation_{-1};
    int colorLocation_{-1};
    glm::mat4 viewProjection_{1.0F};
};

} // namespace engine
