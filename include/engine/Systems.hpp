#pragma once

#include "engine/Components.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine {

struct AABB {
    // 축에 평행한 상자다. center와 양수 half extent를 cm 단위로 저장한다.
    glm::vec3 center{0.0F};
    glm::vec3 extent{0.0F};

    [[nodiscard]] bool intersects(const AABB& other) const;
};

struct HitResult {
    BoxComponent* component{nullptr};
    glm::vec3 location{0.0F};
    glm::vec3 normal{0.0F};
    float distance{0.0F};
};

struct MovementResult {
    glm::vec3 location{0.0F};
    bool blockedX{false};
    bool blockedY{false};
    bool blockedZ{false};
};

enum class OverlapEventType {
    Begin,
    Stay,
    End,
};

struct OverlapEvent {
    Guid first;
    Guid second;
    OverlapEventType type{OverlapEventType::Begin};
};

class CollisionWorld {
public:
    /// World의 BoxComponent를 순회하는 stateless query와 overlap 프레임 상태를 제공한다.
    [[nodiscard]] AABB bounds(const BoxComponent& box) const;
    [[nodiscard]] std::vector<BoxComponent*> overlap(
        const AABB& area,
        const World& world,
        CollisionChannel queryChannel = CollisionChannel::WorldDynamic
    ) const;
    [[nodiscard]] std::optional<HitResult> raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float distance,
        const World& world,
        CollisionChannel queryChannel = CollisionChannel::Visibility
    ) const;
    [[nodiscard]] std::optional<HitResult> sweep(
        const AABB& shape,
        const glm::vec3& delta,
        const World& world,
        CollisionChannel queryChannel = CollisionChannel::WorldDynamic,
        const BoxComponent* ignored = nullptr
    ) const;
    [[nodiscard]] MovementResult moveComponent(
        BoxComponent& moving,
        const glm::vec3& delta,
        const World& world
    ) const;
    void updateOverlaps(const World& world);
    [[nodiscard]] const std::vector<OverlapEvent>& overlapEvents() const;

private:
    struct Pair {
        Guid first;
        Guid second;
        constexpr auto operator<=>(const Pair&) const = default;
    };

    [[nodiscard]] CollisionResponse responseBetween(
        const BoxComponent& first,
        const BoxComponent& second
    ) const;
    [[nodiscard]] bool blocksAt(
        const BoxComponent& moving,
        const glm::vec3& location,
        const World& world
    ) const;
    [[nodiscard]] AABB boundsAt(
        const BoxComponent& box,
        const glm::vec3& location
    ) const;

    std::vector<Pair> previousOverlaps_;
    std::vector<OverlapEvent> overlapEvents_;
};

class RenderScene {
public:
    /// Component revision을 비교해 렌더러가 읽을 값 복사본만 보관한다.
    void sync(const World& world);
    [[nodiscard]] const std::vector<RenderProxy>& proxies() const;
    [[nodiscard]] const std::vector<DirectionalLightProxy>& lights() const;
    [[nodiscard]] std::size_t updatesLastSync() const;

private:
    std::unordered_map<Guid, RenderProxy> proxyMap_;
    std::vector<RenderProxy> proxies_;
    std::vector<DirectionalLightProxy> lights_;
    std::size_t updatesLastSync_{0};
};

enum class Key {
    W,
    A,
    S,
    D,
    Escape,
    F5,
};

class InputSystem {
public:
    /// 플랫폼 키를 게임플레이 축 이름으로 변환한다. 키 자체의 수명은 Application에 있다.
    void bindAxis(std::string name, Key positive, Key negative);
    void setKeyDown(Key key, bool down);
    [[nodiscard]] bool keyDown(Key key) const;
    [[nodiscard]] float axis(std::string_view name) const;

private:
    struct AxisBinding {
        Key positive;
        Key negative;
    };

    std::unordered_map<std::string, AxisBinding> axes_;
    std::unordered_map<Key, bool> keys_;
};

class OutputLog {
public:
    void write(std::string message);
    [[nodiscard]] const std::vector<std::string>& messages() const;

private:
    std::vector<std::string> messages_;
};

struct AssetData {
    Guid guid;
    std::string name;
    std::string type;
    std::filesystem::path source;
};

struct StaticMeshAsset {
    Guid guid;
    MeshPrimitive primitive{MeshPrimitive::Cube};
};

struct MaterialAsset {
    Guid guid;
    MaterialInstance material;
};

class AssetRegistry {
public:
    /// Content 아래 .meta를 스캔하며 GPU 리소스는 소유하지 않는다.
    void scan(const std::filesystem::path& contentRoot, OutputLog* log = nullptr);
    [[nodiscard]] const std::vector<AssetData>& assets() const;
    [[nodiscard]] const AssetData* find(Guid guid) const;
    [[nodiscard]] const AssetData* findByName(std::string_view name) const;
    [[nodiscard]] std::vector<const AssetData*> findByType(
        std::string_view type
    ) const;
    [[nodiscard]] std::optional<StaticMeshAsset> loadStaticMesh(
        Guid guid,
        OutputLog* log = nullptr
    ) const;
    [[nodiscard]] std::optional<MaterialAsset> loadMaterial(
        Guid guid,
        OutputLog* log = nullptr
    ) const;

private:
    std::vector<AssetData> assets_;
};

class WorldSerializer {
public:
    /// reflection의 Serializable 프로퍼티와 GUID/attachment를 JSON으로 왕복한다.
    [[nodiscard]] static std::string toJson(const World& world);
    [[nodiscard]] static std::unique_ptr<World> fromJson(
        std::string_view text,
        OutputLog* log = nullptr
    );
    static bool save(const World& world, const std::filesystem::path& path);
    [[nodiscard]] static std::unique_ptr<World> load(
        const std::filesystem::path& path,
        OutputLog* log = nullptr
    );
    /// World 객체 주소는 유지하고 내부 Actor와 시스템 상태만 JSON으로 교체한다.
    static bool restore(
        World& world,
        std::string_view text,
        OutputLog* log = nullptr
    );
    /// Actor와 소유 Component에 새 GUID를 부여해 같은 World에 복제한다.
    [[nodiscard]] static Actor* duplicateActor(
        World& world,
        const Actor& source,
        std::string name,
        OutputLog* log = nullptr
    );
};

struct PropertyChange {
    Guid object;
    std::string property;
    PropertyValue before;
    PropertyValue after;
};

class TransactionStack {
public:
    /// 프로퍼티 묶음 또는 World snapshot을 한 번의 Undo/Redo 단위로 저장한다.
    void record(
        Guid object,
        std::string property,
        PropertyValue before,
        PropertyValue after
    );
    void recordGroup(std::vector<PropertyChange> changes);
    void recordTransform(
        Guid object,
        const Transform& before,
        const Transform& after
    );
    void recordSnapshot(std::string before, std::string after);
    bool undo(World& world);
    bool redo(World& world);
    void clear();

private:
    struct Transaction {
        std::vector<PropertyChange> changes;
        std::string beforeSnapshot;
        std::string afterSnapshot;
    };

    bool apply(World& world, const Transaction& transaction, bool useAfter);

    std::vector<Transaction> undo_;
    std::vector<Transaction> redo_;
};

} // namespace engine
