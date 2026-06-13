#pragma once

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace engine {

class Object;

/// 저장과 에셋 참조에 사용하는 128비트 식별자다. 이름이나 경로와 독립적이다.
class Guid {
public:
    constexpr Guid() = default;
    constexpr Guid(std::uint64_t high, std::uint64_t low)
        : values_{high, low} {}

    [[nodiscard]] static Guid create();
    [[nodiscard]] static std::optional<Guid> parse(std::string_view text);
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool valid() const;

    constexpr auto operator<=>(const Guid&) const = default;

private:
    std::array<std::uint64_t, 2> values_{};
};

struct Transform {
    // 좌표 규칙: X는 앞, Y는 오른쪽, Z는 위이며 거리 1단위는 1cm다.
    // 회전은 degree 단위 Euler 각으로 저장한다.
    glm::vec3 location{0.0F};
    glm::vec3 rotationDegrees{0.0F};
    glm::vec3 scale{1.0F};

    [[nodiscard]] glm::mat4 matrix() const;
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up() const;

    [[nodiscard]] static Transform fromMatrix(const glm::mat4& matrix);
    [[nodiscard]] static Transform combine(
        const Transform& parent,
        const Transform& local
    );
};

enum class TickGroup {
    PrePhysics,
    Physics,
    PostPhysics,
    PostUpdate,
};

struct TickSettings {
    // Tick은 기본 비활성화다. interval이 0이면 해당 그룹의 매 고정 update에 돈다.
    bool enabled{false};
    TickGroup group{TickGroup::PrePhysics};
    float interval{0.0F};
};

enum class PropertyType {
    Boolean,
    Integer,
    Float,
    String,
    Vector3,
    Guid,
};

using PropertyValue =
    std::variant<std::monostate, bool, int, float, std::string, glm::vec3, Guid>;

enum class PropertyFlags : std::uint8_t {
    None = 0,
    Editable = 1 << 0,
    Serializable = 1 << 1,
};

constexpr PropertyFlags operator|(PropertyFlags first, PropertyFlags second) {
    return static_cast<PropertyFlags>(
        static_cast<std::uint8_t>(first) | static_cast<std::uint8_t>(second)
    );
}

constexpr bool hasFlag(PropertyFlags value, PropertyFlags flag) {
    return (
        static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)
    ) != 0;
}

struct PropertyDescriptor {
    // Details와 저장기가 공유하는 프로퍼티 접근 정보다.
    std::string name;
    std::string category;
    PropertyType type{PropertyType::String};
    PropertyFlags flags{PropertyFlags::None};
    std::function<PropertyValue(const Object&)> getter;
    std::function<bool(Object&, const PropertyValue&)> setter;
};

using ObjectFactory =
    std::function<std::unique_ptr<Object>(Object* outer, std::string name)>;

struct TypeDescriptor {
    // 코드 생성 없이 타입 상속, 생성 방법, 프로퍼티 목록을 명시한다.
    std::string name;
    const TypeDescriptor* parent{nullptr};
    ObjectFactory factory;
    std::vector<PropertyDescriptor> properties;

    [[nodiscard]] bool isA(const TypeDescriptor& base) const;
    [[nodiscard]] std::vector<const PropertyDescriptor*> allProperties() const;
};

class ReflectionRegistry {
public:
    /// 프로그램 전체가 공유하는 타입 저장소다. registerEngineTypes에서 채운다.
    static ReflectionRegistry& instance();

    const TypeDescriptor& registerType(TypeDescriptor descriptor);
    [[nodiscard]] const TypeDescriptor* find(std::string_view name) const;
    [[nodiscard]] const std::vector<std::unique_ptr<TypeDescriptor>>& types() const;

private:
    std::vector<std::unique_ptr<TypeDescriptor>> types_;
    std::unordered_map<std::string, TypeDescriptor*> byName_;
};

class Object {
public:
    /// outer는 논리적 소유자를 가리키며 메모리를 소유하지 않는다.
    explicit Object(std::string name = {}, Object* outer = nullptr);
    virtual ~Object() = default;

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    [[nodiscard]] virtual std::string_view typeName() const;
    [[nodiscard]] const TypeDescriptor* typeDescriptor() const;

    [[nodiscard]] Guid guid() const;
    void setGuid(Guid guid);

    [[nodiscard]] const std::string& name() const;
    void setName(std::string name);

    [[nodiscard]] Object* outer() const;
    void setOuter(Object* outer);

private:
    Guid guid_{Guid::create()};
    std::string name_;
    Object* outer_{nullptr};
};

void registerEngineTypes();

} // namespace engine

namespace std {

template<>
struct hash<engine::Guid> {
    size_t operator()(const engine::Guid& guid) const noexcept {
        return hash<string>{}(guid.toString());
    }
};

} // namespace std
