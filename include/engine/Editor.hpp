#pragma once

#include "engine/RenderTypes.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

class RenderScene;

struct EditorRay {
    glm::vec3 origin{0.0F};
    glm::vec3 direction{1.0F, 0.0F, 0.0F};
};

/// 한 프레임 동안 카메라가 해석할 입력 값이다. deltaTime은 초 단위다.
struct EditorViewportInput {
    float deltaTime{0.0F};
    glm::vec2 mouseDelta{0.0F};
    float mouseWheel{0.0F};
    bool look{false};
    bool orbit{false};
    bool pan{false};
    bool moveForward{false};
    bool moveBackward{false};
    bool moveRight{false};
    bool moveLeft{false};
    bool moveUp{false};
    bool moveDown{false};
    bool fast{false};
};

/// World에 저장되지 않는 에디터 전용 카메라다. 모든 거리 값은 cm를 사용한다.
class EditorViewportController {
public:
    EditorViewportController();

    void update(const EditorViewportInput& input);
    void setPivot(const glm::vec3& pivot);
    void focus(const glm::vec3& center, float radius);

    [[nodiscard]] CameraView cameraView(float aspectRatio) const;
    [[nodiscard]] EditorRay screenRay(
        const glm::vec2& pixel,
        const glm::vec2& viewportSize
    ) const;

    [[nodiscard]] const glm::vec3& position() const;
    [[nodiscard]] const glm::vec3& pivot() const;
    [[nodiscard]] float yawDegrees() const;
    [[nodiscard]] float pitchDegrees() const;

private:
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;

    glm::vec3 position_{-900.0F, -900.0F, 1050.0F};
    glm::vec3 pivot_{0.0F, 0.0F, 40.0F};
    float yawDegrees_{45.0F};
    float pitchDegrees_{-38.5F};
    float moveSpeed_{900.0F};
};

struct ProxyPickResult {
    Guid componentGuid;
    float distance{0.0F};
};

/// visible RenderProxy의 로컬 단위 큐브와 ray를 검사해 가장 가까운 결과를 돌려준다.
[[nodiscard]] std::optional<ProxyPickResult> pickRenderProxy(
    const RenderScene& scene,
    const EditorRay& ray
);

enum class ScreenshotTarget {
    Editor,
    Viewport,
    Both,
};

struct ScreenshotRequest {
    std::filesystem::path path;
    ScreenshotTarget target{ScreenshotTarget::Both};
    int frame{2};
    bool exitAfterCapture{false};
};

struct ScreenshotResult {
    bool success{false};
    std::vector<std::filesystem::path> images;
    std::vector<std::filesystem::path> metadata;
    std::string error;
};

struct ApplicationOptions {
    int windowWidth{1440};
    int windowHeight{810};
    bool hidden{false};
    std::optional<ScreenshotRequest> screenshot;
};

/// GUI와 자동 캡처가 같은 실행 파일을 사용하도록 명령줄 옵션을 해석한다.
[[nodiscard]] ApplicationOptions parseApplicationOptions(
    std::span<const std::string_view> arguments
);

/// OpenGL readback처럼 왼쪽 아래가 원점인 RGBA 데이터도 PNG 방향으로 변환해 저장한다.
[[nodiscard]] bool writePngRgba(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const unsigned char> pixels,
    bool originBottomLeft,
    std::string* error = nullptr
);

} // namespace engine
