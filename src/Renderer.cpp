#include "engine/Renderer.hpp"

#include "engine/Systems.hpp"

#include <glad/gl.h>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#ifndef ENGINE_SHADER_DIR
#define ENGINE_SHADER_DIR L"shaders"
#endif

namespace engine {

namespace {

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return {
        reinterpret_cast<const char*>(utf8.data()),
        utf8.size(),
    };
}

} // namespace

ViewportRenderTarget::ViewportRenderTarget() {
    glGenFramebuffers(1, &framebuffer_);
    glGenTextures(1, &colorTexture_);
    glGenRenderbuffers(1, &depthRenderbuffer_);
}

ViewportRenderTarget::~ViewportRenderTarget() {
    glDeleteRenderbuffers(1, &depthRenderbuffer_);
    glDeleteTextures(1, &colorTexture_);
    glDeleteFramebuffers(1, &framebuffer_);
}

bool ViewportRenderTarget::resize(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width_ == width && height_ == height) {
        return true;
    }

    width_ = width;
    height_ = height;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        colorTexture_,
        0
    );

    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer_);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,
        width_,
        height_
    );
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        depthRenderbuffer_
    );

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

void ViewportRenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
}

void ViewportRenderTarget::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int ViewportRenderTarget::colorTexture() const {
    return colorTexture_;
}

int ViewportRenderTarget::width() const {
    return width_;
}

int ViewportRenderTarget::height() const {
    return height_;
}

std::vector<unsigned char> ViewportRenderTarget::readRgba() const {
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width_ * height_ * 4)
    );
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0,
        0,
        width_,
        height_,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return pixels;
}

Renderer::Renderer()
    : program_(createProgram()) {
    mvpLocation_ = glGetUniformLocation(program_, "uMVP");
    modelLocation_ = glGetUniformLocation(program_, "uModel");
    colorLocation_ = glGetUniformLocation(program_, "uColor");
    lightDirectionLocation_ = glGetUniformLocation(program_, "uLightDirection");
    lightColorLocation_ = glGetUniformLocation(program_, "uLightColor");
    lightIntensityLocation_ = glGetUniformLocation(program_, "uLightIntensity");
    ambientLocation_ = glGetUniformLocation(program_, "uAmbient");
    lightingEnabledLocation_ = glGetUniformLocation(program_, "uLightingEnabled");
    createCubeMesh();
    createGridMesh(1000.0F, 100.0F);
}

Renderer::~Renderer() {
    for (const auto& [guid, resource] : textureCache_) {
        (void)guid;
        glDeleteTextures(1, &resource.texture);
    }
    glDeleteBuffers(1, &gridVbo_);
    glDeleteVertexArrays(1, &gridVao_);
    glDeleteBuffers(1, &cubeEbo_);
    glDeleteBuffers(1, &cubeVbo_);
    glDeleteVertexArrays(1, &cubeVao_);
    glDeleteProgram(program_);
}

void Renderer::beginFrame(
    const CameraView& camera,
    int framebufferWidth,
    int framebufferHeight,
    int viewportX,
    int viewportY,
    int viewportWidth,
    int viewportHeight
) {
    viewProjection_ = camera.viewProjection();
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.08F, 0.10F, 0.14F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(
        viewportX,
        viewportY,
        std::max(viewportWidth, 1),
        std::max(viewportHeight, 1)
    );
}

void Renderer::render(const RenderScene& scene) const {
    if (!scene.lights().empty()) {
        activeLight_ = scene.lights().front();
    } else {
        activeLight_ = DirectionalLightProxy{};
    }

    drawGrid();
    std::size_t opaqueDraws = 1;
    std::size_t debugDraws = 0;
    for (const RenderProxy& proxy : scene.proxies()) {
        if (proxy.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(2.0F);
            ++debugDraws;
        } else {
            ++opaqueDraws;
        }
        drawCubeModel(
            proxy.worldMatrix,
            proxy.material.baseColor,
            !proxy.wireframe
        );
        if (proxy.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    recordPass(RenderPassKind::Opaque, "Opaque", opaqueDraws);
    recordPass(RenderPassKind::Debug, "Debug Wire", debugDraws);
}

void Renderer::renderToTarget(
    ViewportRenderTarget& target,
    const RenderScene& scene,
    const CameraView& camera,
    std::span<const Guid> selectedComponents
) {
    target.bind();
    beginPassRecording();
    recordPass(RenderPassKind::Shadow, "Shadow", 0, false);
    glViewport(0, 0, target.width(), target.height());
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08F, 0.10F, 0.14F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    viewProjection_ = camera.viewProjection();
    render(scene);

    if (!selectedComponents.empty()) {
        const std::unordered_set<Guid> selected{
            selectedComponents.begin(),
            selectedComponents.end(),
        };
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0F);
        std::size_t selectionDraws = 0;
        for (const RenderProxy& proxy : scene.proxies()) {
            if (selected.contains(proxy.componentGuid)) {
                drawCubeModel(
                    glm::scale(proxy.worldMatrix, glm::vec3{1.015F}),
                    {1.0F, 0.72F, 0.12F},
                    false
                );
                ++selectionDraws;
            }
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0F);
        recordPass(RenderPassKind::Debug, "Selection Overlay", selectionDraws);
    } else {
        recordPass(RenderPassKind::Debug, "Selection Overlay", 0);
    }
    recordPass(RenderPassKind::UI, "Dear ImGui UI", lastUiDrawCount_);
    ViewportRenderTarget::unbind();
}

const std::vector<RenderPassRecord>& Renderer::lastPasses() const {
    return lastPasses_;
}

void Renderer::recordUiPass(std::size_t drawCount) {
    lastUiDrawCount_ = drawCount;
    const auto found = std::find_if(
        lastPasses_.begin(),
        lastPasses_.end(),
        [](const RenderPassRecord& pass) {
            return pass.kind == RenderPassKind::UI;
        }
    );
    if (found != lastPasses_.end()) {
        found->drawCount = drawCount;
        return;
    }

    recordPass(RenderPassKind::UI, "Dear ImGui UI", drawCount);
}

void Renderer::beginPassRecording() const {
    lastPasses_.clear();
}

void Renderer::recordPass(
    RenderPassKind kind,
    std::string name,
    std::size_t drawCount,
    bool implemented
) const {
    lastPasses_.push_back({
        kind,
        std::move(name),
        drawCount,
        implemented,
    });
}

void Renderer::clearBackbuffer(int width, int height) const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, std::max(width, 1), std::max(height, 1));
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.045F, 0.052F, 0.065F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
}

std::vector<unsigned char> Renderer::readBackbufferRgba(
    int width,
    int height
) const {
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width * height * 4)
    );
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(
        0,
        0,
        width,
        height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );
    return pixels;
}

const MeshGpuResource* Renderer::meshFor(
    const StaticMeshAsset& asset,
    std::string* error
) {
    const auto cached = meshCache_.find(asset.guid);
    if (cached != meshCache_.end()) {
        return &cached->second;
    }

    if (asset.primitive != MeshPrimitive::Cube) {
        if (error != nullptr) {
            *error = "Unsupported StaticMesh primitive.";
        }
        return nullptr;
    }

    auto [iterator, inserted] = meshCache_.emplace(
        asset.guid,
        MeshGpuResource{
            asset.guid,
            asset.primitive,
            cubeVao_,
            36,
        }
    );
    (void)inserted;
    return &iterator->second;
}

const TextureGpuResource* Renderer::textureFor(
    const TextureAsset& asset,
    std::string* error
) {
    const auto cached = textureCache_.find(asset.guid);
    if (cached != textureCache_.end()) {
        return &cached->second;
    }

    stbi_set_flip_vertically_on_load(1);
    int width = 0;
    int height = 0;
    int channels = 0;
    const std::string utf8Path = pathToUtf8(asset.source);
    unsigned char* pixels = stbi_load(
        utf8Path.c_str(),
        &width,
        &height,
        &channels,
        4
    );
    if (pixels == nullptr) {
        if (error != nullptr) {
            *error = "Could not load texture image: " + utf8Path;
        }
        return nullptr;
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    auto [iterator, inserted] = textureCache_.emplace(
        asset.guid,
        TextureGpuResource{
            asset.guid,
            texture,
            width,
            height,
            channels,
        }
    );
    (void)inserted;
    return &iterator->second;
}

void Renderer::drawCubeModel(
    const glm::mat4& model,
    const glm::vec3& color,
    bool lit
) const {
    const glm::mat4 mvp = viewProjection_ * model;

    glUseProgram(program_);
    glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLocation_, 1, glm::value_ptr(color));
    glUniform3fv(
        lightDirectionLocation_,
        1,
        glm::value_ptr(activeLight_.direction)
    );
    glUniform3fv(lightColorLocation_, 1, glm::value_ptr(activeLight_.color));
    glUniform1f(lightIntensityLocation_, activeLight_.intensity);
    glUniform1f(ambientLocation_, 0.22F);
    glUniform1i(lightingEnabledLocation_, lit ? 1 : 0);
    glBindVertexArray(cubeVao_);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void Renderer::drawGrid() const {
    glUseProgram(program_);
    const glm::mat4 model{1.0F};
    glUniformMatrix4fv(
        mvpLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(viewProjection_)
    );
    glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
    const glm::vec3 color{0.38F, 0.44F, 0.48F};
    glUniform3fv(colorLocation_, 1, glm::value_ptr(color));
    glUniform1i(lightingEnabledLocation_, 0);
    glBindVertexArray(gridVao_);
    glDrawArrays(GL_LINES, 0, gridVertexCount_);
}

unsigned int Renderer::compileShader(unsigned int type, const std::string& source) {
    const unsigned int shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log);
    }

    return shader;
}

std::string Renderer::readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open a shader file.");
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

unsigned int Renderer::createProgram() {
    const std::filesystem::path shaderDirectory{ENGINE_SHADER_DIR};
    const unsigned int vertexShader = compileShader(
        GL_VERTEX_SHADER,
        readTextFile(shaderDirectory / "basic.vert")
    );
    const unsigned int fragmentShader = compileShader(
        GL_FRAGMENT_SHADER,
        readTextFile(shaderDirectory / "basic.frag")
    );

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Shader link failed:\n" + log);
    }

    return program;
}

void Renderer::createCubeMesh() {
    constexpr float vertices[] = {
        -0.5F, -0.5F, -0.5F,
         0.5F, -0.5F, -0.5F,
         0.5F,  0.5F, -0.5F,
        -0.5F,  0.5F, -0.5F,
        -0.5F, -0.5F,  0.5F,
         0.5F, -0.5F,  0.5F,
         0.5F,  0.5F,  0.5F,
        -0.5F,  0.5F,  0.5F,
    };

    constexpr unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 6, 5, 6, 4, 7,
        0, 4, 5, 5, 1, 0,
        3, 2, 6, 6, 7, 3,
        1, 5, 6, 6, 2, 1,
        0, 3, 7, 7, 4, 0,
    };

    glGenVertexArrays(1, &cubeVao_);
    glGenBuffers(1, &cubeVbo_);
    glGenBuffers(1, &cubeEbo_);

    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
}

void Renderer::createGridMesh(float halfExtent, float spacing) {
    std::vector<float> vertices;
    const int lineCount = static_cast<int>(halfExtent / spacing);

    for (int line = -lineCount; line <= lineCount; ++line) {
        const float coordinate = static_cast<float>(line) * spacing;

        vertices.insert(
            vertices.end(),
            {-halfExtent, coordinate, 1.0F, halfExtent, coordinate, 1.0F}
        );
        vertices.insert(
            vertices.end(),
            {coordinate, -halfExtent, 1.0F, coordinate, halfExtent, 1.0F}
        );
    }

    gridVertexCount_ = static_cast<int>(vertices.size() / 3);
    glGenVertexArrays(1, &gridVao_);
    glGenBuffers(1, &gridVbo_);
    glBindVertexArray(gridVao_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
}

} // namespace engine
