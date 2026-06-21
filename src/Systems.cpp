#include "engine/Systems.hpp"

#include <nlohmann/json.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace {

using Json = nlohmann::json;

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return {
        reinterpret_cast<const char*>(utf8.data()),
        utf8.size(),
    };
}

Json vectorToJson(const glm::vec3& value) {
    return Json::array({value.x, value.y, value.z});
}

glm::vec3 vectorFromJson(const Json& value) {
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

Json propertyToJson(const engine::PropertyValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using Type = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Type, std::monostate>) {
                return nullptr;
            } else if constexpr (std::is_same_v<Type, glm::vec3>) {
                return vectorToJson(item);
            } else if constexpr (std::is_same_v<Type, engine::Guid>) {
                return item.toString();
            } else {
                return item;
            }
        },
        value
    );
}

engine::PropertyValue propertyFromJson(
    engine::PropertyType type,
    const Json& value
) {
    using engine::PropertyType;
    switch (type) {
    case PropertyType::Boolean:
        return value.get<bool>();
    case PropertyType::Integer:
        return value.get<int>();
    case PropertyType::Float:
        return value.get<float>();
    case PropertyType::String:
        return value.get<std::string>();
    case PropertyType::Vector3:
        return vectorFromJson(value);
    case PropertyType::Guid: {
        const auto parsed = engine::Guid::parse(value.get<std::string>());
        return parsed.value_or(engine::Guid{});
    }
    }
    return {};
}

std::optional<Json> readJsonFile(
    const std::filesystem::path& path,
    engine::OutputLog* log
) {
    std::ifstream stream(path);
    if (!stream) {
        if (log != nullptr) {
            log->write("Could not open asset file: " + pathToUtf8(path));
        }
        return std::nullopt;
    }

    try {
        Json json;
        stream >> json;
        return json;
    } catch (const std::exception& exception) {
        if (log != nullptr) {
            log->write(
                "Could not parse asset file " + pathToUtf8(path) + ": " +
                exception.what()
            );
        }
        return std::nullopt;
    }
}

engine::MeshPrimitive meshPrimitiveFromText(std::string_view text) {
    if (text == "Cube") {
        return engine::MeshPrimitive::Cube;
    }
    return engine::MeshPrimitive::Cube;
}

glm::vec3 jsonVectorOr(
    const Json& json,
    std::string_view key,
    const glm::vec3& fallback
) {
    const auto found = json.find(std::string{key});
    if (found == json.end() || !found->is_array() || found->size() != 3) {
        return fallback;
    }
    return {
        found->at(0).get<float>(),
        found->at(1).get<float>(),
        found->at(2).get<float>(),
    };
}

std::string lowerAscii(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))
        );
    }
    return text;
}

std::string extensionLower(const std::filesystem::path& path) {
    return lowerAscii(path.extension().string());
}

std::filesystem::path uniqueAssetPath(
    const std::filesystem::path& directory,
    const std::filesystem::path& preferredName
) {
    std::filesystem::path candidate = directory / preferredName;
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    const std::filesystem::path stem = preferredName.stem();
    const std::filesystem::path extension = preferredName.extension();
    for (int suffix = 2;; ++suffix) {
        candidate = directory /
                    (stem.wstring() + L" " + std::to_wstring(suffix) +
                     extension.wstring());
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
}

void logImportFailure(
    engine::AssetImportResult& result,
    std::string message,
    engine::OutputLog* log
) {
    result.success = false;
    result.error = std::move(message);
    if (log != nullptr) {
        log->write(result.error);
    }
}

engine::Guid guidFromExistingMeta(const std::filesystem::path& metaPath) {
    const std::optional<Json> meta = readJsonFile(metaPath, nullptr);
    if (!meta.has_value() || !meta->contains("guid")) {
        return engine::Guid::create();
    }
    return engine::Guid::parse(meta->value("guid", ""))
        .value_or(engine::Guid::create());
}

bool writeJsonFile(const std::filesystem::path& path, const Json& json) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    if (!stream) {
        return false;
    }
    stream << json.dump(2);
    return true;
}

Json serializeProperties(const engine::Object& object) {
    Json result = Json::object();
    const engine::TypeDescriptor* type = object.typeDescriptor();
    if (type == nullptr) {
        return result;
    }

    for (const engine::PropertyDescriptor* property : type->allProperties()) {
        if (engine::hasFlag(
                property->flags,
                engine::PropertyFlags::Serializable
            ) &&
            property->getter) {
            result[property->name] = propertyToJson(property->getter(object));
        }
    }
    return result;
}

void applyProperties(
    engine::Object& object,
    const Json& values,
    engine::OutputLog* log
) {
    const engine::TypeDescriptor* type = object.typeDescriptor();
    if (type == nullptr || !values.is_object()) {
        return;
    }

    std::map<std::string, const engine::PropertyDescriptor*> properties;
    for (const engine::PropertyDescriptor* property : type->allProperties()) {
        properties.emplace(property->name, property);
    }

    for (auto iterator = values.begin(); iterator != values.end(); ++iterator) {
        const auto found = properties.find(iterator.key());
        if (found == properties.end()) {
            if (log != nullptr) {
                log->write(
                    "Skipped unknown property '" + iterator.key() +
                    "' on " + object.name()
                );
            }
            continue;
        }

        const engine::PropertyDescriptor& property = *found->second;
        if (property.setter) {
            property.setter(
                object,
                propertyFromJson(property.type, iterator.value())
            );
        }
    }
}

engine::Guid parseGuid(const Json& value) {
    return engine::Guid::parse(value.get<std::string>())
        .value_or(engine::Guid{});
}

} // namespace

namespace engine {

bool AABB::intersects(const AABB& other) const {
    const glm::vec3 distance = glm::abs(center - other.center);
    const glm::vec3 combined = extent + other.extent;
    return distance.x < combined.x &&
           distance.y < combined.y &&
           distance.z < combined.z;
}

AABB CollisionWorld::bounds(const BoxComponent& box) const {
    return boundsAt(box, box.worldTransform().location);
}

std::vector<BoxComponent*> CollisionWorld::overlap(
    const AABB& area,
    const World& world,
    CollisionChannel queryChannel
) const {
    std::vector<BoxComponent*> result;
    for (BoxComponent* box : world.componentsOfType<BoxComponent>()) {
        if (box->collisionEnabled() &&
            box->responseTo(queryChannel) != CollisionResponse::Ignore &&
            area.intersects(bounds(*box))) {
            result.push_back(box);
        }
    }
    return result;
}

std::optional<HitResult> CollisionWorld::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float distance,
    const World& world,
    CollisionChannel queryChannel
) const {
    if (distance <= 0.0F || glm::dot(direction, direction) <= 0.000001F) {
        return std::nullopt;
    }

    const glm::vec3 rayDirection = glm::normalize(direction);
    std::optional<HitResult> closest;
    float closestDistance = distance;

    for (BoxComponent* box : world.componentsOfType<BoxComponent>()) {
        if (!box->collisionEnabled() ||
            box->responseTo(queryChannel) == CollisionResponse::Ignore) {
            continue;
        }

        const AABB area = bounds(*box);
        const glm::vec3 minimum = area.center - area.extent;
        const glm::vec3 maximum = area.center + area.extent;
        float nearDistance = 0.0F;
        float farDistance = distance;
        glm::vec3 normal{0.0F};

        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(rayDirection[axis]) < 0.000001F) {
                if (origin[axis] < minimum[axis] ||
                    origin[axis] > maximum[axis]) {
                    nearDistance = farDistance + 1.0F;
                    break;
                }
                continue;
            }

            float first = (minimum[axis] - origin[axis]) / rayDirection[axis];
            float second = (maximum[axis] - origin[axis]) / rayDirection[axis];
            float sign = -1.0F;
            if (first > second) {
                std::swap(first, second);
                sign = 1.0F;
            }
            if (first > nearDistance) {
                nearDistance = first;
                normal = glm::vec3{0.0F};
                normal[axis] = sign;
            }
            farDistance = std::min(farDistance, second);
            if (nearDistance > farDistance) {
                break;
            }
        }

        if (nearDistance <= farDistance &&
            nearDistance >= 0.0F &&
            nearDistance < closestDistance) {
            closestDistance = nearDistance;
            closest = HitResult{
                box,
                origin + rayDirection * nearDistance,
                normal,
                nearDistance,
            };
        }
    }
    return closest;
}

std::optional<HitResult> CollisionWorld::sweep(
    const AABB& shape,
    const glm::vec3& delta,
    const World& world,
    CollisionChannel queryChannel,
    const BoxComponent* ignored
) const {
    // v0.5 sweep는 이동 구간을 작은 단계로 샘플링하는 교육용 구현이다.
    // 빠른 물체의 완전한 연속 충돌 검출을 보장하지 않는다.
    constexpr int steps = 64;
    const float distance = glm::length(delta);
    if (distance <= 0.000001F) {
        return std::nullopt;
    }

    for (int step = 1; step <= steps; ++step) {
        const float alpha = static_cast<float>(step) /
                            static_cast<float>(steps);
        AABB candidate = shape;
        candidate.center += delta * alpha;
        for (BoxComponent* box : world.componentsOfType<BoxComponent>()) {
            if (box == ignored ||
                !box->collisionEnabled() ||
                box->responseTo(queryChannel) != CollisionResponse::Block ||
                !candidate.intersects(bounds(*box))) {
                continue;
            }

            glm::vec3 normal{0.0F};
            const glm::vec3 direction = glm::normalize(delta);
            const int axis =
                std::abs(direction.x) > std::abs(direction.y)
                    ? (std::abs(direction.x) > std::abs(direction.z) ? 0 : 2)
                    : (std::abs(direction.y) > std::abs(direction.z) ? 1 : 2);
            normal[axis] = direction[axis] > 0.0F ? -1.0F : 1.0F;
            return HitResult{
                box,
                candidate.center,
                normal,
                distance * alpha,
            };
        }
    }
    return std::nullopt;
}

MovementResult CollisionWorld::moveComponent(
    BoxComponent& moving,
    const glm::vec3& delta,
    const World& world
) const {
    MovementResult result;
    Transform transform = moving.relativeTransform();
    result.location = transform.location;

    // 축을 따로 시도하면 막힌 축만 멈추고 나머지 축으로 미끄러질 수 있다.
    const auto attemptAxis = [&](int axis, bool& blocked) {
        glm::vec3 candidate = result.location;
        candidate[axis] += delta[axis];
        if (blocksAt(moving, candidate, world)) {
            blocked = std::abs(delta[axis]) > 0.000001F;
            return;
        }
        result.location = candidate;
    };

    attemptAxis(0, result.blockedX);
    attemptAxis(1, result.blockedY);
    attemptAxis(2, result.blockedZ);
    moving.setRelativeLocation(result.location);
    return result;
}

void CollisionWorld::updateOverlaps(const World& world) {
    std::vector<Pair> current;
    const auto boxes = world.componentsOfType<BoxComponent>();
    for (std::size_t first = 0; first < boxes.size(); ++first) {
        if (!boxes[first]->collisionEnabled()) {
            continue;
        }
        for (std::size_t second = first + 1; second < boxes.size(); ++second) {
            if (!boxes[second]->collisionEnabled() ||
                responseBetween(*boxes[first], *boxes[second]) !=
                    CollisionResponse::Overlap ||
                !bounds(*boxes[first]).intersects(bounds(*boxes[second]))) {
                continue;
            }
            // GUID 순서를 정규화해야 A-B와 B-A를 같은 overlap으로 비교할 수 있다.
            Pair pair{boxes[first]->guid(), boxes[second]->guid()};
            if (pair.second < pair.first) {
                std::swap(pair.first, pair.second);
            }
            current.push_back(pair);
        }
    }
    std::sort(current.begin(), current.end());

    overlapEvents_.clear();
    for (const Pair& pair : current) {
        const bool existed = std::binary_search(
            previousOverlaps_.begin(),
            previousOverlaps_.end(),
            pair
        );
        overlapEvents_.push_back({
            pair.first,
            pair.second,
            existed ? OverlapEventType::Stay : OverlapEventType::Begin,
        });
    }
    for (const Pair& pair : previousOverlaps_) {
        if (!std::binary_search(current.begin(), current.end(), pair)) {
            overlapEvents_.push_back(
                {pair.first, pair.second, OverlapEventType::End}
            );
        }
    }
    previousOverlaps_ = std::move(current);
}

const std::vector<OverlapEvent>& CollisionWorld::overlapEvents() const {
    return overlapEvents_;
}

CollisionResponse CollisionWorld::responseBetween(
    const BoxComponent& first,
    const BoxComponent& second
) const {
    const CollisionResponse a = first.responseTo(second.objectChannel());
    const CollisionResponse b = second.responseTo(first.objectChannel());
    if (a == CollisionResponse::Ignore || b == CollisionResponse::Ignore) {
        return CollisionResponse::Ignore;
    }
    if (a == CollisionResponse::Overlap || b == CollisionResponse::Overlap) {
        return CollisionResponse::Overlap;
    }
    return CollisionResponse::Block;
}

bool CollisionWorld::blocksAt(
    const BoxComponent& moving,
    const glm::vec3& location,
    const World& world
) const {
    const AABB movingBounds = boundsAt(moving, location);
    for (BoxComponent* other : world.componentsOfType<BoxComponent>()) {
        if (other == &moving ||
            !other->collisionEnabled() ||
            responseBetween(moving, *other) != CollisionResponse::Block) {
            continue;
        }
        if (movingBounds.intersects(bounds(*other))) {
            return true;
        }
    }
    return false;
}

AABB CollisionWorld::boundsAt(
    const BoxComponent& box,
    const glm::vec3& location
) const {
    const Transform transform = box.worldTransform();
    return {
        location,
        box.extent() * glm::abs(transform.scale),
    };
}

void RenderScene::sync(const World& world) {
    updatesLastSync_ = 0;
    std::unordered_set<Guid> live;
    for (PrimitiveComponent* component :
         world.componentsOfType<PrimitiveComponent>()) {
        const auto proxy = component->createRenderProxy();
        if (!proxy.has_value()) {
            proxyMap_.erase(component->guid());
            continue;
        }

        live.insert(component->guid());
        // revision이 같은 proxy는 재생성하지 않아 게임 데이터 변경 비용만 반영한다.
        const auto found = proxyMap_.find(component->guid());
        if (found == proxyMap_.end() ||
            found->second.revision != proxy->revision) {
            proxyMap_[component->guid()] = *proxy;
            ++updatesLastSync_;
        }
    }
    std::erase_if(proxyMap_, [&live](const auto& entry) {
        return !live.contains(entry.first);
    });

    proxies_.clear();
    proxies_.reserve(proxyMap_.size());
    for (const auto& [guid, proxy] : proxyMap_) {
        (void)guid;
        proxies_.push_back(proxy);
    }

    lights_.clear();
    for (DirectionalLightComponent* light :
         world.componentsOfType<DirectionalLightComponent>()) {
        lights_.push_back(light->createLightProxy());
    }
}

const std::vector<RenderProxy>& RenderScene::proxies() const {
    return proxies_;
}

const std::vector<DirectionalLightProxy>& RenderScene::lights() const {
    return lights_;
}

std::size_t RenderScene::updatesLastSync() const {
    return updatesLastSync_;
}

void InputSystem::bindAxis(std::string name, Key positive, Key negative) {
    axes_.insert_or_assign(std::move(name), AxisBinding{positive, negative});
}

void InputSystem::setKeyDown(Key key, bool down) {
    keys_[key] = down;
}

bool InputSystem::keyDown(Key key) const {
    const auto found = keys_.find(key);
    return found != keys_.end() && found->second;
}

float InputSystem::axis(std::string_view name) const {
    const auto found = axes_.find(std::string{name});
    if (found == axes_.end()) {
        return 0.0F;
    }
    return (keyDown(found->second.positive) ? 1.0F : 0.0F) -
           (keyDown(found->second.negative) ? 1.0F : 0.0F);
}

void OutputLog::write(std::string message) {
    messages_.push_back(std::move(message));
}

const std::vector<std::string>& OutputLog::messages() const {
    return messages_;
}

AssetImportResult AssetImporter::importGltfAsStaticMesh(
    const std::filesystem::path& source,
    const std::filesystem::path& contentRoot,
    OutputLog* log
) {
    AssetImportResult result;
    if (!std::filesystem::exists(source)) {
        logImportFailure(result, "glTF import source does not exist.", log);
        return result;
    }

    const std::string extension = extensionLower(source);
    if (extension != ".gltf" && extension != ".glb") {
        logImportFailure(result, "glTF import requires .gltf or .glb.", log);
        return result;
    }
    if (extension == ".gltf") {
        const std::optional<Json> gltf = readJsonFile(source, log);
        if (!gltf.has_value() || !gltf->contains("asset")) {
            logImportFailure(result, "glTF file is missing asset metadata.", log);
            return result;
        }
    }

    const std::filesystem::path directory =
        contentRoot / "Imported" / "Meshes";
    std::filesystem::create_directories(directory);
    const std::filesystem::path assetJson = uniqueAssetPath(
        directory,
        source.stem().wstring() + L".asset.json"
    );
    const std::filesystem::path copiedSource = uniqueAssetPath(
        directory,
        source.filename()
    );

    std::error_code copyError;
    std::filesystem::copy_file(
        source,
        copiedSource,
        std::filesystem::copy_options::overwrite_existing,
        copyError
    );
    if (copyError) {
        logImportFailure(result, "Could not copy glTF source file.", log);
        return result;
    }

    std::filesystem::path meta = assetJson;
    meta += L".meta";
    const Guid guid = guidFromExistingMeta(meta);
    const std::string name = pathToUtf8(source.stem());
    const Json asset{
        {"type", "StaticMesh"},
        {"primitive", "Cube"},
        {"source", pathToUtf8(copiedSource.filename())},
        {"sourceFormat", extension == ".glb" ? "glb" : "glTF"},
    };
    const Json metadata{
        {"guid", guid.toString()},
        {"name", name},
        {"type", "StaticMesh"},
        {"source", pathToUtf8(assetJson.filename())},
    };
    if (!writeJsonFile(assetJson, asset) || !writeJsonFile(meta, metadata)) {
        logImportFailure(result, "Could not write imported glTF metadata.", log);
        return result;
    }

    result.success = true;
    result.asset = AssetData{guid, name, "StaticMesh", assetJson};
    result.copiedSource = copiedSource;
    result.metadata = meta;
    return result;
}

AssetImportResult AssetImporter::importTexture(
    const std::filesystem::path& source,
    const std::filesystem::path& contentRoot,
    OutputLog* log
) {
    AssetImportResult result;
    if (!std::filesystem::exists(source)) {
        logImportFailure(result, "Texture import source does not exist.", log);
        return result;
    }

    const std::string extension = extensionLower(source);
    if (extension != ".png" && extension != ".jpg" &&
        extension != ".jpeg" && extension != ".bmp" &&
        extension != ".tga") {
        logImportFailure(result, "Texture import requires an image file.", log);
        return result;
    }

    const std::filesystem::path directory =
        contentRoot / "Imported" / "Textures";
    std::filesystem::create_directories(directory);
    const std::filesystem::path textureJson = uniqueAssetPath(
        directory,
        source.stem().wstring() + L".texture.json"
    );
    const std::filesystem::path copiedSource = uniqueAssetPath(
        directory,
        source.filename()
    );

    std::error_code copyError;
    std::filesystem::copy_file(
        source,
        copiedSource,
        std::filesystem::copy_options::overwrite_existing,
        copyError
    );
    if (copyError) {
        logImportFailure(result, "Could not copy texture source file.", log);
        return result;
    }

    std::filesystem::path meta = textureJson;
    meta += L".meta";
    const Guid guid = guidFromExistingMeta(meta);
    const std::string name = pathToUtf8(source.stem());
    const Json asset{
        {"type", "Texture"},
        {"source", pathToUtf8(copiedSource.filename())},
        {"sourceFormat", extension.substr(1)},
    };
    const Json metadata{
        {"guid", guid.toString()},
        {"name", name},
        {"type", "Texture"},
        {"source", pathToUtf8(textureJson.filename())},
    };
    if (!writeJsonFile(textureJson, asset) || !writeJsonFile(meta, metadata)) {
        logImportFailure(result, "Could not write imported texture metadata.", log);
        return result;
    }

    result.success = true;
    result.asset = AssetData{guid, name, "Texture", textureJson};
    result.copiedSource = copiedSource;
    result.metadata = meta;
    return result;
}

void AssetRegistry::scan(
    const std::filesystem::path& contentRoot,
    OutputLog* log
) {
    assets_.clear();
    if (!std::filesystem::exists(contentRoot)) {
        return;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(contentRoot)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".meta") {
            continue;
        }
        try {
            std::ifstream stream(entry.path());
            Json meta;
            stream >> meta;
            AssetData asset;
            asset.guid = parseGuid(meta.at("guid"));
            asset.name = meta.value(
                "name",
                pathToUtf8(entry.path().stem())
            );
            asset.type = meta.value("type", "Unknown");
            asset.source = entry.path().parent_path() /
                           meta.value("source", std::string{});
            assets_.push_back(std::move(asset));
        } catch (const std::exception& exception) {
            if (log != nullptr) {
                log->write(
                    "Could not read " + pathToUtf8(entry.path()) + ": " +
                    exception.what()
                );
            }
        }
    }
}

const std::vector<AssetData>& AssetRegistry::assets() const {
    return assets_;
}

const AssetData* AssetRegistry::find(Guid guid) const {
    const auto found = std::find_if(
        assets_.begin(),
        assets_.end(),
        [guid](const AssetData& asset) { return asset.guid == guid; }
    );
    return found == assets_.end() ? nullptr : &*found;
}

const AssetData* AssetRegistry::findByName(std::string_view name) const {
    const auto found = std::find_if(
        assets_.begin(),
        assets_.end(),
        [name](const AssetData& asset) { return asset.name == name; }
    );
    return found == assets_.end() ? nullptr : &*found;
}

std::vector<const AssetData*> AssetRegistry::findByType(
    std::string_view type
) const {
    std::vector<const AssetData*> result;
    for (const AssetData& asset : assets_) {
        if (asset.type == type) {
            result.push_back(&asset);
        }
    }
    return result;
}

std::optional<StaticMeshAsset> AssetRegistry::loadStaticMesh(
    Guid guid,
    OutputLog* log
) const {
    const AssetData* asset = find(guid);
    if (asset == nullptr || asset->type != "StaticMesh") {
        if (log != nullptr) {
            log->write("StaticMesh asset GUID was not found.");
        }
        return std::nullopt;
    }

    const std::optional<Json> json = readJsonFile(asset->source, log);
    if (!json.has_value()) {
        return std::nullopt;
    }
    return StaticMeshAsset{
        asset->guid,
        meshPrimitiveFromText(json->value("primitive", "Cube")),
    };
}

std::optional<MaterialAsset> AssetRegistry::loadMaterial(
    Guid guid,
    OutputLog* log
) const {
    const AssetData* asset = find(guid);
    if (asset == nullptr || asset->type != "Material") {
        if (log != nullptr) {
            log->write("Material asset GUID was not found.");
        }
        return std::nullopt;
    }

    const std::optional<Json> json = readJsonFile(asset->source, log);
    if (!json.has_value()) {
        return std::nullopt;
    }

    MaterialInstance material;
    material.baseColor = jsonVectorOr(
        *json,
        "baseColor",
        material.baseColor
    );
    material.roughness = json->value("roughness", material.roughness);
    material.metallic = json->value("metallic", material.metallic);
    return MaterialAsset{asset->guid, material};
}

std::optional<TextureAsset> AssetRegistry::loadTexture(
    Guid guid,
    OutputLog* log
) const {
    const AssetData* asset = find(guid);
    if (asset == nullptr || asset->type != "Texture") {
        if (log != nullptr) {
            log->write("Texture asset GUID was not found.");
        }
        return std::nullopt;
    }

    const std::optional<Json> json = readJsonFile(asset->source, log);
    if (!json.has_value()) {
        return std::nullopt;
    }

    const std::string source = json->value("source", std::string{});
    if (source.empty()) {
        if (log != nullptr) {
            log->write("Texture asset has no source image.");
        }
        return std::nullopt;
    }
    return TextureAsset{
        asset->guid,
        asset->source.parent_path() / source,
        json->value("sourceFormat", std::string{}),
    };
}

std::string WorldSerializer::toJson(const World& world) {
    Json root{
        {"version", 1},
        {"type", world.typeName()},
        {"guid", world.guid().toString()},
        {"name", world.name()},
        {"actors", Json::array()},
    };

    for (const auto& actor : world.actors()) {
        Json actorJson{
            {"type", actor->typeName()},
            {"guid", actor->guid().toString()},
            {"name", actor->name()},
            {"root", actor->rootComponent() == nullptr
                         ? Json(nullptr)
                         : Json(actor->rootComponent()->guid().toString())},
            {"properties", serializeProperties(*actor)},
            {"components", Json::array()},
        };

        for (const auto& component : actor->components()) {
            auto* scene = dynamic_cast<SceneComponent*>(component.get());
            actorJson["components"].push_back({
                {"type", component->typeName()},
                {"guid", component->guid().toString()},
                {"name", component->name()},
                {"parent", scene == nullptr || scene->parent() == nullptr
                               ? Json(nullptr)
                               : Json(scene->parent()->guid().toString())},
                {"properties", serializeProperties(*component)},
            });
        }
        root["actors"].push_back(std::move(actorJson));
    }
    return root.dump(2);
}

std::unique_ptr<World> WorldSerializer::fromJson(
    std::string_view text,
    OutputLog* log
) {
    const Json root = Json::parse(text);
    auto world = std::make_unique<World>(root.value("name", "World"));
    if (root.contains("guid")) {
        world->setGuid(parseGuid(root["guid"]));
    }

    // 객체를 모두 만든 다음 GUID로 계층을 연결한다. JSON에서 부모가 자식보다
    // 뒤에 나와도 안전하게 복원하기 위한 두 단계 로딩이다.
    struct PendingScene {
        SceneComponent* component;
        Guid parent;
    };
    std::vector<PendingScene> pendingParents;
    std::vector<std::pair<Actor*, Guid>> pendingRoots;
    std::vector<std::pair<Actor*, Json>> pendingActorProperties;

    for (const Json& actorJson : root.value("actors", Json::array())) {
        const std::string typeName = actorJson.value("type", "Actor");
        const TypeDescriptor* actorType =
            ReflectionRegistry::instance().find(typeName);
        if (actorType == nullptr) {
            if (log != nullptr) {
                log->write("Skipped unknown actor type '" + typeName + "'.");
            }
            continue;
        }

        Actor* actor = world->spawnActorByType(
            *actorType,
            actorJson.value("name", typeName),
            false
        );
        if (actor == nullptr) {
            continue;
        }
        actor->setGuid(parseGuid(actorJson.at("guid")));
        pendingActorProperties.emplace_back(
            actor,
            actorJson.value("properties", Json::object())
        );

        for (const Json& componentJson :
             actorJson.value("components", Json::array())) {
            const std::string componentTypeName =
                componentJson.value("type", "ActorComponent");
            const TypeDescriptor* componentType =
                ReflectionRegistry::instance().find(componentTypeName);
            if (componentType == nullptr) {
                if (log != nullptr) {
                    log->write(
                        "Skipped unknown component type '" +
                        componentTypeName + "'."
                    );
                }
                continue;
            }

            ActorComponent* component = actor->addComponentByType(
                *componentType,
                componentJson.value("name", componentTypeName)
            );
            if (component == nullptr) {
                continue;
            }
            component->setGuid(parseGuid(componentJson.at("guid")));
            applyProperties(
                *component,
                componentJson.value("properties", Json::object()),
                log
            );

            if (auto* scene = dynamic_cast<SceneComponent*>(component);
                scene != nullptr && componentJson.contains("parent") &&
                !componentJson["parent"].is_null()) {
                pendingParents.push_back(
                    {scene, parseGuid(componentJson["parent"])}
                );
            }
        }

        if (actorJson.contains("root") && !actorJson["root"].is_null()) {
            pendingRoots.emplace_back(actor, parseGuid(actorJson["root"]));
        }
    }

    for (const PendingScene& pending : pendingParents) {
        auto* parent =
            dynamic_cast<SceneComponent*>(world->findObject(pending.parent));
        pending.component->attachTo(parent);
    }
    for (const auto& [actor, rootGuid] : pendingRoots) {
        actor->setRootComponent(
            dynamic_cast<SceneComponent*>(world->findObject(rootGuid))
        );
    }
    for (auto& [actor, properties] : pendingActorProperties) {
        applyProperties(*actor, properties, log);
    }
    return world;
}

bool WorldSerializer::save(
    const World& world,
    const std::filesystem::path& path
) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    if (!stream) {
        return false;
    }
    stream << toJson(world);
    return true;
}

std::unique_ptr<World> WorldSerializer::load(
    const std::filesystem::path& path,
    OutputLog* log
) {
    std::ifstream stream(path);
    if (!stream) {
        return nullptr;
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    return fromJson(contents.str(), log);
}

bool WorldSerializer::restore(
    World& world,
    std::string_view text,
    OutputLog* log
) {
    std::unique_ptr<World> loaded;
    try {
        loaded = fromJson(text, log);
    } catch (const std::exception& exception) {
        if (log != nullptr) {
            log->write(
                "Could not restore World snapshot: " +
                std::string{exception.what()}
            );
        }
        return false;
    }
    if (loaded == nullptr) {
        return false;
    }

    InputSystem* input = world.inputSystem_;
    world.clearForLoad();
    world.setGuid(loaded->guid());
    world.setName(loaded->name());
    world.actors_ = std::move(loaded->actors_);
    world.pendingSpawns_.clear();
    world.collisionWorld_ = std::move(loaded->collisionWorld_);
    world.renderScene_ = std::move(loaded->renderScene_);
    world.inputSystem_ = input;
    world.beganPlay_ = false;
    world.ticking_ = false;

    for (const auto& actor : world.actors_) {
        actor->world_ = &world;
        actor->setOuter(&world);
    }
    return true;
}

Actor* WorldSerializer::duplicateActor(
    World& world,
    const Actor& source,
    std::string name,
    OutputLog* log
) {
    Json root = Json::parse(toJson(world));
    const Json* sourceJson = nullptr;
    for (const Json& actorJson : root.at("actors")) {
        if (parseGuid(actorJson.at("guid")) == source.guid()) {
            sourceJson = &actorJson;
            break;
        }
    }
    if (sourceJson == nullptr) {
        return nullptr;
    }

    Json clone = *sourceJson;
    clone["name"] = std::move(name);
    std::unordered_map<std::string, std::string> remappedGuids;
    const std::string oldActorGuid = clone.at("guid").get<std::string>();
    const std::string newActorGuid = Guid::create().toString();
    remappedGuids.emplace(oldActorGuid, newActorGuid);
    clone["guid"] = newActorGuid;

    for (Json& component : clone.at("components")) {
        const std::string oldGuid = component.at("guid").get<std::string>();
        const std::string newGuid = Guid::create().toString();
        remappedGuids.emplace(oldGuid, newGuid);
        component["guid"] = newGuid;
    }
    const auto remap = [&remappedGuids](Json& value) {
        if (!value.is_string()) {
            return;
        }
        const auto found = remappedGuids.find(value.get<std::string>());
        if (found != remappedGuids.end()) {
            value = found->second;
        }
    };
    remap(clone["root"]);
    for (Json& component : clone.at("components")) {
        remap(component["parent"]);
    }

    Json temporaryRoot{
        {"version", 1},
        {"type", "World"},
        {"guid", Guid::create().toString()},
        {"name", "DuplicateActorTemporaryWorld"},
        {"actors", Json::array({clone})},
    };
    auto temporary = fromJson(temporaryRoot.dump(), log);
    if (temporary == nullptr || temporary->actors_.empty()) {
        return nullptr;
    }

    std::unique_ptr<Actor> duplicate = std::move(temporary->actors_.front());
    temporary->actors_.clear();
    // 임시 World의 등록 상태를 새 World로 가져가면 onRegister가 생략된다.
    duplicate->unregisterComponents();
    duplicate->world_ = &world;
    duplicate->setOuter(&world);
    Actor* result = world.adoptActor(std::move(duplicate), false);
    applyProperties(
        *result,
        clone.value("properties", Json::object()),
        log
    );
    return result;
}

void TransactionStack::record(
    Guid object,
    std::string property,
    PropertyValue before,
    PropertyValue after
) {
    recordGroup({{
        object,
        std::move(property),
        std::move(before),
        std::move(after),
    }});
}

void TransactionStack::recordGroup(std::vector<PropertyChange> changes) {
    if (changes.empty()) {
        return;
    }
    undo_.push_back({std::move(changes), {}, {}});
    redo_.clear();
}

void TransactionStack::recordTransform(
    Guid object,
    const Transform& before,
    const Transform& after
) {
    constexpr float epsilon = 0.0001F;
    if (glm::length(before.location - after.location) < epsilon &&
        glm::length(before.rotationDegrees - after.rotationDegrees) < epsilon &&
        glm::length(before.scale - after.scale) < epsilon) {
        return;
    }
    recordGroup({
        {object, "Location", before.location, after.location},
        {object, "Rotation", before.rotationDegrees, after.rotationDegrees},
        {object, "Scale", before.scale, after.scale},
    });
}

void TransactionStack::recordSnapshot(
    std::string before,
    std::string after
) {
    if (before == after) {
        return;
    }
    undo_.push_back({{}, std::move(before), std::move(after)});
    redo_.clear();
}

bool TransactionStack::undo(World& world) {
    if (undo_.empty()) {
        return false;
    }
    Transaction transaction = std::move(undo_.back());
    undo_.pop_back();
    if (!apply(world, transaction, false)) {
        undo_.push_back(std::move(transaction));
        return false;
    }
    redo_.push_back(std::move(transaction));
    return true;
}

bool TransactionStack::redo(World& world) {
    if (redo_.empty()) {
        return false;
    }
    Transaction transaction = std::move(redo_.back());
    redo_.pop_back();
    if (!apply(world, transaction, true)) {
        redo_.push_back(std::move(transaction));
        return false;
    }
    undo_.push_back(std::move(transaction));
    return true;
}

void TransactionStack::clear() {
    undo_.clear();
    redo_.clear();
}

bool TransactionStack::apply(
    World& world,
    const Transaction& transaction,
    bool useAfter
) {
    if (!transaction.beforeSnapshot.empty() ||
        !transaction.afterSnapshot.empty()) {
        return WorldSerializer::restore(
            world,
            useAfter ? transaction.afterSnapshot : transaction.beforeSnapshot
        );
    }

    bool applied = true;
    for (const PropertyChange& change : transaction.changes) {
        Object* object = world.findObject(change.object);
        if (object == nullptr || object->typeDescriptor() == nullptr) {
            applied = false;
            continue;
        }

        bool found = false;
        for (const PropertyDescriptor* property :
             object->typeDescriptor()->allProperties()) {
            if (property->name == change.property && property->setter) {
                found = property->setter(
                    *object,
                    useAfter ? change.after : change.before
                );
                break;
            }
        }
        applied = applied && found;
    }
    return applied;
}

} // namespace engine
