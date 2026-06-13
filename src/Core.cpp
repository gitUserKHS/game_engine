#include "engine/Core.hpp"

#include <glm/common.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <atomic>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <utility>

namespace {

std::uint64_t randomSeed() {
    const auto now = std::chrono::high_resolution_clock::now()
                         .time_since_epoch()
                         .count();
    std::random_device device;
    return static_cast<std::uint64_t>(now) ^
           (static_cast<std::uint64_t>(device()) << 32U) ^
           static_cast<std::uint64_t>(device());
}

} // namespace

namespace engine {

Guid Guid::create() {
    static std::mt19937_64 generator{randomSeed()};
    static std::mutex mutex;
    static std::atomic<std::uint64_t> sequence{1};

    std::scoped_lock lock{mutex};
    return Guid{
        generator(),
        generator() ^ sequence.fetch_add(1, std::memory_order_relaxed),
    };
}

std::optional<Guid> Guid::parse(std::string_view text) {
    if (text.size() != 32) {
        return std::nullopt;
    }

    std::uint64_t values[2]{};
    for (int index = 0; index < 2; ++index) {
        const char* begin = text.data() + index * 16;
        const char* end = begin + 16;
        const auto [position, error] =
            std::from_chars(begin, end, values[index], 16);
        if (error != std::errc{} || position != end) {
            return std::nullopt;
        }
    }
    return Guid{values[0], values[1]};
}

std::string Guid::toString() const {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0')
           << std::setw(16) << values_[0]
           << std::setw(16) << values_[1];
    return stream.str();
}

bool Guid::valid() const {
    return values_[0] != 0 || values_[1] != 0;
}

glm::mat4 Transform::matrix() const {
    // 열 벡터 규칙에서 오른쪽 연산이 먼저 적용되므로 최종 순서는
    // scale -> X/Y/Z rotation -> translation이다.
    glm::mat4 result{1.0F};
    result = glm::translate(result, location);
    result = glm::rotate(
        result,
        glm::radians(rotationDegrees.z),
        {0.0F, 0.0F, 1.0F}
    );
    result = glm::rotate(
        result,
        glm::radians(rotationDegrees.y),
        {0.0F, 1.0F, 0.0F}
    );
    result = glm::rotate(
        result,
        glm::radians(rotationDegrees.x),
        {1.0F, 0.0F, 0.0F}
    );
    return glm::scale(result, scale);
}

glm::vec3 Transform::forward() const {
    return glm::normalize(glm::vec3{matrix() * glm::vec4{1, 0, 0, 0}});
}

glm::vec3 Transform::right() const {
    return glm::normalize(glm::vec3{matrix() * glm::vec4{0, 1, 0, 0}});
}

glm::vec3 Transform::up() const {
    return glm::normalize(glm::vec3{matrix() * glm::vec4{0, 0, 1, 0}});
}

Transform Transform::fromMatrix(const glm::mat4& matrix) {
    // attachment를 합친 행렬을 다시 편집 가능한 위치/회전/크기로 분해한다.
    // skew와 perspective는 이 학습용 Transform에서 지원하지 않아 버린다.
    Transform result;
    glm::quat orientation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(
        matrix,
        result.scale,
        orientation,
        result.location,
        skew,
        perspective
    );
    result.rotationDegrees = glm::degrees(glm::eulerAngles(orientation));
    return result;
}

Transform Transform::combine(
    const Transform& parent,
    const Transform& local
) {
    return fromMatrix(parent.matrix() * local.matrix());
}

bool TypeDescriptor::isA(const TypeDescriptor& base) const {
    for (const TypeDescriptor* current = this; current != nullptr;
         current = current->parent) {
        if (current == &base) {
            return true;
        }
    }
    return false;
}

std::vector<const PropertyDescriptor*> TypeDescriptor::allProperties() const {
    std::vector<const PropertyDescriptor*> result;
    if (parent != nullptr) {
        result = parent->allProperties();
    }
    for (const PropertyDescriptor& property : properties) {
        result.push_back(&property);
    }
    return result;
}

ReflectionRegistry& ReflectionRegistry::instance() {
    static ReflectionRegistry registry;
    return registry;
}

const TypeDescriptor& ReflectionRegistry::registerType(
    TypeDescriptor descriptor
) {
    if (const TypeDescriptor* existing = find(descriptor.name)) {
        return *existing;
    }

    auto stored = std::make_unique<TypeDescriptor>(std::move(descriptor));
    TypeDescriptor* pointer = stored.get();
    byName_.emplace(pointer->name, pointer);
    types_.push_back(std::move(stored));
    return *pointer;
}

const TypeDescriptor* ReflectionRegistry::find(std::string_view name) const {
    const auto found = byName_.find(std::string{name});
    return found == byName_.end() ? nullptr : found->second;
}

const std::vector<std::unique_ptr<TypeDescriptor>>&
ReflectionRegistry::types() const {
    return types_;
}

Object::Object(std::string name, Object* outer)
    : name_(std::move(name)),
      outer_(outer) {}

std::string_view Object::typeName() const {
    return "Object";
}

const TypeDescriptor* Object::typeDescriptor() const {
    return ReflectionRegistry::instance().find(typeName());
}

Guid Object::guid() const {
    return guid_;
}

void Object::setGuid(Guid guid) {
    guid_ = guid;
}

const std::string& Object::name() const {
    return name_;
}

void Object::setName(std::string name) {
    name_ = std::move(name);
}

Object* Object::outer() const {
    return outer_;
}

void Object::setOuter(Object* outer) {
    outer_ = outer;
}

} // namespace engine
