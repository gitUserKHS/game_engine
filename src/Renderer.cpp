#include "engine/Renderer.hpp"

#include "engine/Systems.hpp"

#include <glad/gl.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifndef ENGINE_SHADER_DIR
#define ENGINE_SHADER_DIR "shaders"
#endif

namespace engine {

Renderer::Renderer()
    : program_(createProgram()) {
    mvpLocation_ = glGetUniformLocation(program_, "uMVP");
    colorLocation_ = glGetUniformLocation(program_, "uColor");
    createCubeMesh();
    createGridMesh(1000.0F, 100.0F);
}

Renderer::~Renderer() {
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
    drawGrid();
    for (const RenderProxy& proxy : scene.proxies()) {
        if (proxy.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(2.0F);
        }
        drawCubeModel(proxy.worldMatrix, proxy.material.baseColor);
        if (proxy.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
}

void Renderer::drawCubeModel(
    const glm::mat4& model,
    const glm::vec3& color
) const {
    const glm::mat4 mvp = viewProjection_ * model;

    glUseProgram(program_);
    glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3fv(colorLocation_, 1, glm::value_ptr(color));
    glBindVertexArray(cubeVao_);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void Renderer::drawGrid() const {
    glUseProgram(program_);
    glUniformMatrix4fv(
        mvpLocation_,
        1,
        GL_FALSE,
        glm::value_ptr(viewProjection_)
    );
    const glm::vec3 color{0.38F, 0.44F, 0.48F};
    glUniform3fv(colorLocation_, 1, glm::value_ptr(color));
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

std::string Renderer::readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open shader file: " + path);
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

unsigned int Renderer::createProgram() {
    const std::string shaderDirectory = ENGINE_SHADER_DIR;
    const unsigned int vertexShader = compileShader(
        GL_VERTEX_SHADER,
        readTextFile(shaderDirectory + "/basic.vert")
    );
    const unsigned int fragmentShader = compileShader(
        GL_FRAGMENT_SHADER,
        readTextFile(shaderDirectory + "/basic.frag")
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
