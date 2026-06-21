#include "engine/Components.hpp"

#include "engine/Systems.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <algorithm>

namespace engine {

PrimitiveComponent::PrimitiveComponent(std::string name, Actor* owner)
    : SceneComponent(std::move(name), owner) {}

std::string_view PrimitiveComponent::typeName() const {
    return "PrimitiveComponent";
}

bool PrimitiveComponent::visible() const {
    return visible_;
}

void PrimitiveComponent::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        markRenderStateDirty();
    }
}

const MaterialInstance& PrimitiveComponent::material() const {
    return material_;
}

void PrimitiveComponent::setMaterial(const MaterialInstance& material) {
    material_ = material;
    markRenderStateDirty();
}

std::uint64_t PrimitiveComponent::renderRevision() const {
    return renderRevision_;
}

std::optional<RenderProxy> PrimitiveComponent::createRenderProxy() const {
    return std::nullopt;
}

void PrimitiveComponent::onTransformChanged() {
    markRenderStateDirty();
}

void PrimitiveComponent::markRenderStateDirty() {
    ++renderRevision_;
}

StaticMeshComponent::StaticMeshComponent(std::string name, Actor* owner)
    : PrimitiveComponent(std::move(name), owner) {}

std::string_view StaticMeshComponent::typeName() const {
    return "StaticMeshComponent";
}

Guid StaticMeshComponent::meshAsset() const {
    return meshAsset_;
}

void StaticMeshComponent::setMeshAsset(Guid asset) {
    meshAsset_ = asset;
    markRenderStateDirty();
}

std::optional<RenderProxy> StaticMeshComponent::createRenderProxy() const {
    if (!visible()) {
        return std::nullopt;
    }
    return RenderProxy{
        guid(),
        MeshPrimitive::Cube,
        worldMatrix(),
        material(),
        false,
        renderRevision(),
    };
}

BoxComponent::BoxComponent(std::string name, Actor* owner)
    : PrimitiveComponent(std::move(name), owner) {
    responses_.fill(CollisionResponse::Block);
    setVisible(false);
}

std::string_view BoxComponent::typeName() const {
    return "BoxComponent";
}

const glm::vec3& BoxComponent::extent() const {
    return extent_;
}

void BoxComponent::setExtent(const glm::vec3& extent) {
    extent_ = glm::max(glm::abs(extent), glm::vec3{0.01F});
    markRenderStateDirty();
}

bool BoxComponent::collisionEnabled() const {
    return collisionEnabled_;
}

void BoxComponent::setCollisionEnabled(bool enabled) {
    collisionEnabled_ = enabled;
}

bool BoxComponent::drawDebug() const {
    return drawDebug_;
}

void BoxComponent::setDrawDebug(bool enabled) {
    drawDebug_ = enabled;
    markRenderStateDirty();
}

CollisionChannel BoxComponent::objectChannel() const {
    return objectChannel_;
}

void BoxComponent::setObjectChannel(CollisionChannel channel) {
    objectChannel_ = channel;
}

CollisionResponse BoxComponent::responseTo(CollisionChannel channel) const {
    return responses_[static_cast<std::size_t>(channel)];
}

void BoxComponent::setResponse(
    CollisionChannel channel,
    CollisionResponse response
) {
    responses_[static_cast<std::size_t>(channel)] = response;
}

std::optional<RenderProxy> BoxComponent::createRenderProxy() const {
    if (!drawDebug()) {
        return std::nullopt;
    }

    glm::mat4 matrix = worldMatrix();
    matrix = glm::scale(matrix, extent_ * 2.0F);
    MaterialInstance debugMaterial;
    debugMaterial.baseColor = {0.2F, 1.0F, 0.35F};
    return RenderProxy{
        guid(),
        MeshPrimitive::Cube,
        matrix,
        debugMaterial,
        true,
        renderRevision(),
    };
}

CameraComponent::CameraComponent(std::string name, Actor* owner)
    : SceneComponent(std::move(name), owner) {}

std::string_view CameraComponent::typeName() const {
    return "CameraComponent";
}

float CameraComponent::fieldOfView() const {
    return fieldOfView_;
}

void CameraComponent::setFieldOfView(float degrees) {
    fieldOfView_ = std::clamp(degrees, 10.0F, 150.0F);
}

bool CameraComponent::active() const {
    return active_;
}

void CameraComponent::setActive(bool active) {
    active_ = active;
}

CameraView CameraComponent::cameraView(float aspectRatio) const {
    const glm::mat4 matrix = worldMatrix();
    glm::vec3 location = glm::vec3{matrix[3]};
    const glm::vec3 forward = glm::normalize(
        glm::vec3{matrix * glm::vec4{1.0F, 0.0F, 0.0F, 0.0F}}
    );
    const glm::vec3 up = glm::normalize(
        glm::vec3{matrix * glm::vec4{0.0F, 0.0F, 1.0F, 0.0F}}
    );
    if (auto* springArm = dynamic_cast<SpringArmComponent*>(parent());
        springArm != nullptr && owner() != nullptr && owner()->world() != nullptr) {
        const glm::vec3 target = springArm->worldTransform().location;
        const glm::vec3 desired = location - target;
        const float distance = glm::length(desired);
        if (distance > 0.001F) {
            const BoxComponent* ignored = owner()->findComponent<BoxComponent>();
            const auto hit = owner()->world()->collision().raycast(
                target,
                desired,
                distance,
                *owner()->world(),
                CollisionChannel::Camera,
                ignored
            );
            if (hit.has_value()) {
                const glm::vec3 direction = glm::normalize(desired);
                location = target +
                           direction * std::max(hit->distance - 10.0F, 10.0F);
            }
        }
    }
    CameraView view;
    view.view = glm::lookAt(
        location,
        location + forward,
        up
    );
    view.projection = glm::perspective(
        glm::radians(fieldOfView_),
        std::max(aspectRatio, 0.01F),
        10.0F,
        100000.0F
    );
    return view;
}

SpringArmComponent::SpringArmComponent(std::string name, Actor* owner)
    : SceneComponent(std::move(name), owner) {}

std::string_view SpringArmComponent::typeName() const {
    return "SpringArmComponent";
}

float SpringArmComponent::targetArmLength() const {
    return targetArmLength_;
}

void SpringArmComponent::setTargetArmLength(float length) {
    targetArmLength_ = std::max(length, 0.0F);
}

DirectionalLightComponent::DirectionalLightComponent(
    std::string name,
    Actor* owner
) : SceneComponent(std::move(name), owner) {}

std::string_view DirectionalLightComponent::typeName() const {
    return "DirectionalLightComponent";
}

const glm::vec3& DirectionalLightComponent::color() const {
    return color_;
}

void DirectionalLightComponent::setColor(const glm::vec3& color) {
    color_ = glm::max(color, glm::vec3{0.0F});
}

float DirectionalLightComponent::intensity() const {
    return intensity_;
}

void DirectionalLightComponent::setIntensity(float intensity) {
    intensity_ = std::max(intensity, 0.0F);
}

DirectionalLightProxy DirectionalLightComponent::createLightProxy() const {
    return {guid(), -worldTransform().forward(), color_, intensity_};
}

} // namespace engine
