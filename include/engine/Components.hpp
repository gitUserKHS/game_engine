#pragma once

#include "engine/RenderTypes.hpp"
#include "engine/World.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace engine {

enum class CollisionChannel : std::size_t {
    WorldStatic,
    WorldDynamic,
    Pawn,
    Visibility,
    Camera,
    Count,
};

enum class CollisionResponse {
    Ignore,
    Overlap,
    Block,
};

/// 화면에 보일 수 있는 SceneComponent의 기반이다. OpenGL 대신 RenderProxy를 만든다.
class PrimitiveComponent : public SceneComponent {
public:
    PrimitiveComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] bool visible() const;
    void setVisible(bool visible);

    [[nodiscard]] const MaterialInstance& material() const;
    void setMaterial(const MaterialInstance& material);

    [[nodiscard]] std::uint64_t renderRevision() const;
    [[nodiscard]] virtual std::optional<RenderProxy> createRenderProxy() const;

protected:
    void onTransformChanged() override;
    void markRenderStateDirty();

private:
    MaterialInstance material_;
    std::uint64_t renderRevision_{1};
    bool visible_{true};
};

class StaticMeshComponent : public PrimitiveComponent {
public:
    /// 현재 v0.5에서는 Cube primitive를 표시하며 meshAsset GUID를 보존한다.
    StaticMeshComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] Guid meshAsset() const;
    void setMeshAsset(Guid asset);
    [[nodiscard]] std::optional<RenderProxy> createRenderProxy() const override;

private:
    Guid meshAsset_;
};

class BoxComponent : public PrimitiveComponent {
public:
    /// 충돌 query에는 회전을 감싸는 World AABB를 제공한다. extent 단위는 cm다.
    BoxComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;

    [[nodiscard]] const glm::vec3& extent() const;
    void setExtent(const glm::vec3& extent);

    [[nodiscard]] bool collisionEnabled() const;
    void setCollisionEnabled(bool enabled);

    [[nodiscard]] bool drawDebug() const;
    void setDrawDebug(bool enabled);

    [[nodiscard]] CollisionChannel objectChannel() const;
    void setObjectChannel(CollisionChannel channel);
    [[nodiscard]] CollisionResponse responseTo(CollisionChannel channel) const;
    void setResponse(CollisionChannel channel, CollisionResponse response);

    [[nodiscard]] std::optional<RenderProxy> createRenderProxy() const override;

private:
    glm::vec3 extent_{50.0F};
    std::array<CollisionResponse, static_cast<std::size_t>(CollisionChannel::Count)>
        responses_{};
    CollisionChannel objectChannel_{CollisionChannel::WorldDynamic};
    bool collisionEnabled_{true};
    bool drawDebug_{false};
};

class CameraComponent : public SceneComponent {
public:
    /// World Transform에서 view를 만들고 perspective projection을 제공한다.
    CameraComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] float fieldOfView() const;
    void setFieldOfView(float degrees);
    [[nodiscard]] bool active() const;
    void setActive(bool active);
    [[nodiscard]] CameraView cameraView(float aspectRatio) const;

private:
    float fieldOfView_{55.0F};
    bool active_{true};
};

class SpringArmComponent : public SceneComponent {
public:
    /// 카메라를 대상에서 일정 거리 떨어뜨리는 학습용 attachment 지점이다.
    SpringArmComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] float targetArmLength() const;
    void setTargetArmLength(float length);

private:
    float targetArmLength_{900.0F};
};

class DirectionalLightComponent : public SceneComponent {
public:
    /// 방향, 색, 세기를 렌더 값으로 복사한다. 실제 조명 shader는 이후 버전 범위다.
    DirectionalLightComponent(std::string name, Actor* owner);

    [[nodiscard]] std::string_view typeName() const override;
    [[nodiscard]] const glm::vec3& color() const;
    void setColor(const glm::vec3& color);
    [[nodiscard]] float intensity() const;
    void setIntensity(float intensity);
    [[nodiscard]] DirectionalLightProxy createLightProxy() const;

private:
    glm::vec3 color_{1.0F, 0.95F, 0.85F};
    float intensity_{1.5F};
};

} // namespace engine
