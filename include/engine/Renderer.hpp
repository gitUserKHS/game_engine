#pragma once

#include "engine/RenderTypes.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

class RenderScene;
struct StaticMeshAsset;
struct TextureAsset;

struct MeshGpuResource {
    Guid guid;
    MeshPrimitive primitive{MeshPrimitive::Cube};
    unsigned int vertexArray{0};
    int indexCount{0};
};

struct TextureGpuResource {
    Guid guid;
    unsigned int texture{0};
    int width{0};
    int height{0};
    int channels{0};
};

enum class RenderPassKind {
    Shadow,
    Opaque,
    Debug,
    UI,
};

struct RenderPassRecord {
    RenderPassKind kind{RenderPassKind::Opaque};
    std::string name;
    std::size_t drawCount{0};
    bool implemented{true};
};

/// ImGui Viewport에 표시할 color texture와 depth buffer를 소유한다.
class ViewportRenderTarget {
public:
    ViewportRenderTarget();
    ~ViewportRenderTarget();

    ViewportRenderTarget(const ViewportRenderTarget&) = delete;
    ViewportRenderTarget& operator=(const ViewportRenderTarget&) = delete;

    bool resize(int width, int height);
    void bind() const;
    static void unbind();

    [[nodiscard]] unsigned int colorTexture() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] std::vector<unsigned char> readRgba() const;

private:
    unsigned int framebuffer_{0};
    unsigned int colorTexture_{0};
    unsigned int depthRenderbuffer_{0};
    int width_{0};
    int height_{0};
};

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
    void renderToTarget(
        ViewportRenderTarget& target,
        const RenderScene& scene,
        const CameraView& camera,
        std::span<const Guid> selectedComponents = {}
    );
    void clearBackbuffer(int width, int height) const;
    [[nodiscard]] std::vector<unsigned char> readBackbufferRgba(
        int width,
        int height
    ) const;
    [[nodiscard]] const TextureGpuResource* textureFor(
        const TextureAsset& asset,
        std::string* error = nullptr
    );
    [[nodiscard]] const MeshGpuResource* meshFor(
        const StaticMeshAsset& asset,
        std::string* error = nullptr
    );
    [[nodiscard]] const std::vector<RenderPassRecord>& lastPasses() const;
    void recordUiPass(std::size_t drawCount);

private:
    static unsigned int compileShader(unsigned int type, const std::string& source);
    static std::string readTextFile(const std::filesystem::path& path);
    static unsigned int createProgram();
    void createCubeMesh();
    void createGridMesh(float halfExtent, float spacing);
    void drawCubeModel(
        const glm::mat4& model,
        const glm::vec3& color,
        bool lit
    ) const;
    void beginPassRecording() const;
    void recordPass(
        RenderPassKind kind,
        std::string name,
        std::size_t drawCount,
        bool implemented = true
    ) const;

    unsigned int program_{0};
    unsigned int cubeVao_{0};
    unsigned int cubeVbo_{0};
    unsigned int cubeEbo_{0};
    unsigned int gridVao_{0};
    unsigned int gridVbo_{0};
    int gridVertexCount_{0};
    int mvpLocation_{-1};
    int modelLocation_{-1};
    int colorLocation_{-1};
    int lightDirectionLocation_{-1};
    int lightColorLocation_{-1};
    int lightIntensityLocation_{-1};
    int ambientLocation_{-1};
    int lightingEnabledLocation_{-1};
    glm::mat4 viewProjection_{1.0F};
    mutable DirectionalLightProxy activeLight_;
    mutable std::vector<RenderPassRecord> lastPasses_;
    std::size_t lastUiDrawCount_{0};
    std::unordered_map<Guid, MeshGpuResource> meshCache_;
    std::unordered_map<Guid, TextureGpuResource> textureCache_;
};

} // namespace engine
