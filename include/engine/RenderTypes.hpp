#pragma once

#include "engine/Core.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {

enum class MeshPrimitive {
    Cube,
};

struct MaterialInstance {
    glm::vec3 baseColor{1.0F};
    float roughness{0.7F};
    float metallic{0.0F};
};

/// 게임 객체 포인터 없이 렌더에 필요한 값만 담는 한 프레임용 복사본이다.
struct RenderProxy {
    Guid componentGuid;
    MeshPrimitive mesh{MeshPrimitive::Cube};
    glm::mat4 worldMatrix{1.0F};
    MaterialInstance material;
    bool wireframe{false};
    std::uint64_t revision{0};
};

struct DirectionalLightProxy {
    Guid componentGuid;
    glm::vec3 direction{-0.4F, -0.3F, -1.0F};
    glm::vec3 color{1.0F};
    float intensity{1.0F};
};

struct CameraView {
    glm::mat4 view{1.0F};
    glm::mat4 projection{1.0F};

    [[nodiscard]] glm::mat4 viewProjection() const {
        return projection * view;
    }
};

} // namespace engine
