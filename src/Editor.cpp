#include "engine/Editor.hpp"

#include "engine/Systems.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr float kMouseSensitivity = 0.18F;
constexpr float kMinimumPitch = -89.0F;
constexpr float kMaximumPitch = 89.0F;

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

int parsePositiveInt(std::string_view text, std::string_view option) {
    int value = 0;
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
    );
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value <= 0) {
        throw std::invalid_argument(
            std::string{option} + " requires a positive integer."
        );
    }
    return value;
}

} // namespace

namespace engine {

EditorViewportController::EditorViewportController() {
    const glm::vec3 direction = glm::normalize(pivot_ - position_);
    yawDegrees_ = glm::degrees(std::atan2(direction.y, direction.x));
    pitchDegrees_ = glm::degrees(std::asin(direction.z));
}

void EditorViewportController::update(const EditorViewportInput& input) {
    if (input.look || input.orbit) {
        yawDegrees_ += input.mouseDelta.x * kMouseSensitivity;
        pitchDegrees_ = glm::clamp(
            pitchDegrees_ - input.mouseDelta.y * kMouseSensitivity,
            kMinimumPitch,
            kMaximumPitch
        );
    }

    if (input.orbit) {
        const float distance = std::max(glm::distance(position_, pivot_), 1.0F);
        position_ = pivot_ - forward() * distance;
    }

    if (input.pan) {
        const float distance = std::max(glm::distance(position_, pivot_), 100.0F);
        const float scale = distance * 0.0015F;
        const glm::vec3 offset =
            right() * (-input.mouseDelta.x * scale) +
            glm::vec3{0.0F, 0.0F, 1.0F} * (input.mouseDelta.y * scale);
        position_ += offset;
        pivot_ += offset;
    }

    const float speed = moveSpeed_ * (input.fast ? 4.0F : 1.0F);
    glm::vec3 movement{0.0F};
    movement += forward() *
                static_cast<float>(input.moveForward - input.moveBackward);
    movement += right() *
                static_cast<float>(input.moveRight - input.moveLeft);
    movement.z += static_cast<float>(input.moveUp - input.moveDown);
    if (glm::dot(movement, movement) > 0.000001F) {
        movement = glm::normalize(movement) * speed * input.deltaTime;
        position_ += movement;
        pivot_ += movement;
    }

    if (std::abs(input.mouseWheel) > 0.0001F) {
        const glm::vec3 offset =
            forward() * input.mouseWheel * std::max(speed * 0.18F, 40.0F);
        position_ += offset;
        pivot_ += offset;
    }
}

void EditorViewportController::setPivot(const glm::vec3& pivot) {
    pivot_ = pivot;
}

void EditorViewportController::focus(const glm::vec3& center, float radius) {
    pivot_ = center;
    const float distance = std::max(radius * 2.5F, 200.0F);
    position_ = pivot_ - forward() * distance;
}

CameraView EditorViewportController::cameraView(float aspectRatio) const {
    CameraView result;
    result.view = glm::lookAt(
        position_,
        position_ + forward(),
        glm::vec3{0.0F, 0.0F, 1.0F}
    );
    result.projection = glm::perspective(
        glm::radians(50.0F),
        std::max(aspectRatio, 0.01F),
        10.0F,
        100000.0F
    );
    return result;
}

EditorRay EditorViewportController::screenRay(
    const glm::vec2& pixel,
    const glm::vec2& viewportSize
) const {
    if (viewportSize.x <= 0.0F || viewportSize.y <= 0.0F) {
        return {position_, forward()};
    }

    const glm::vec2 ndc{
        pixel.x / viewportSize.x * 2.0F - 1.0F,
        1.0F - pixel.y / viewportSize.y * 2.0F,
    };
    const CameraView camera = cameraView(viewportSize.x / viewportSize.y);
    const glm::mat4 inverse = glm::inverse(camera.viewProjection());
    glm::vec4 nearPoint = inverse * glm::vec4{ndc, -1.0F, 1.0F};
    glm::vec4 farPoint = inverse * glm::vec4{ndc, 1.0F, 1.0F};
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    return {
        position_,
        glm::normalize(glm::vec3{farPoint - nearPoint}),
    };
}

const glm::vec3& EditorViewportController::position() const {
    return position_;
}

const glm::vec3& EditorViewportController::pivot() const {
    return pivot_;
}

float EditorViewportController::yawDegrees() const {
    return yawDegrees_;
}

float EditorViewportController::pitchDegrees() const {
    return pitchDegrees_;
}

glm::vec3 EditorViewportController::forward() const {
    const float yaw = glm::radians(yawDegrees_);
    const float pitch = glm::radians(pitchDegrees_);
    return glm::normalize(glm::vec3{
        std::cos(pitch) * std::cos(yaw),
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
    });
}

glm::vec3 EditorViewportController::right() const {
    return glm::normalize(
        glm::cross(forward(), glm::vec3{0.0F, 0.0F, 1.0F})
    );
}

std::optional<ProxyPickResult> pickRenderProxy(
    const RenderScene& scene,
    const EditorRay& ray
) {
    std::optional<ProxyPickResult> closest;
    float closestDistance = std::numeric_limits<float>::max();

    for (const RenderProxy& proxy : scene.proxies()) {
        const glm::mat4 inverse = glm::inverse(proxy.worldMatrix);
        const glm::vec3 localOrigin =
            glm::vec3{inverse * glm::vec4{ray.origin, 1.0F}};
        const glm::vec3 localDirection =
            glm::vec3{inverse * glm::vec4{ray.direction, 0.0F}};
        float nearDistance = 0.0F;
        float farDistance = closestDistance;

        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(localDirection[axis]) < 0.000001F) {
                if (localOrigin[axis] < -0.5F ||
                    localOrigin[axis] > 0.5F) {
                    nearDistance = farDistance + 1.0F;
                    break;
                }
                continue;
            }
            float first = (-0.5F - localOrigin[axis]) / localDirection[axis];
            float second = (0.5F - localOrigin[axis]) / localDirection[axis];
            if (first > second) {
                std::swap(first, second);
            }
            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);
            if (nearDistance > farDistance) {
                break;
            }
        }

        if (nearDistance <= farDistance && nearDistance >= 0.0F &&
            nearDistance < closestDistance) {
            closestDistance = nearDistance;
            closest = ProxyPickResult{proxy.componentGuid, nearDistance};
        }
    }
    return closest;
}

ApplicationOptions parseApplicationOptions(
    std::span<const std::string_view> arguments
) {
    ApplicationOptions options;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        const auto requireValue = [&]() -> std::string_view {
            if (index + 1 >= arguments.size()) {
                throw std::invalid_argument(
                    std::string{argument} + " requires a value."
                );
            }
            return arguments[++index];
        };

        if (argument == "--capture") {
            if (!options.screenshot.has_value()) {
                options.screenshot = ScreenshotRequest{};
            }
            options.screenshot->path = pathFromUtf8(requireValue());
        } else if (argument == "--capture-target") {
            if (!options.screenshot.has_value()) {
                options.screenshot = ScreenshotRequest{};
            }
            const std::string_view target = requireValue();
            if (target == "editor") {
                options.screenshot->target = ScreenshotTarget::Editor;
            } else if (target == "viewport") {
                options.screenshot->target = ScreenshotTarget::Viewport;
            } else if (target == "both") {
                options.screenshot->target = ScreenshotTarget::Both;
            } else {
                throw std::invalid_argument(
                    "--capture-target must be editor, viewport, or both."
                );
            }
        } else if (argument == "--capture-frame") {
            if (!options.screenshot.has_value()) {
                options.screenshot = ScreenshotRequest{};
            }
            options.screenshot->frame =
                parsePositiveInt(requireValue(), argument);
        } else if (argument == "--exit-after-capture") {
            if (!options.screenshot.has_value()) {
                options.screenshot = ScreenshotRequest{};
            }
            options.screenshot->exitAfterCapture = true;
        } else if (argument == "--window-size") {
            const std::string_view size = requireValue();
            const std::size_t separator = size.find('x');
            if (separator == std::string_view::npos) {
                throw std::invalid_argument(
                    "--window-size must use WIDTHxHEIGHT."
                );
            }
            options.windowWidth =
                parsePositiveInt(size.substr(0, separator), argument);
            options.windowHeight =
                parsePositiveInt(size.substr(separator + 1), argument);
        } else if (argument == "--hidden") {
            options.hidden = true;
        } else {
            throw std::invalid_argument(
                "Unknown command line option: " + std::string{argument}
            );
        }
    }

    if (options.screenshot.has_value() &&
        options.screenshot->path.empty()) {
        throw std::invalid_argument(
            "Screenshot options require --capture <path>."
        );
    }
    return options;
}

} // namespace engine
