#include "engine/Systems.hpp"

#include <nlohmann/json.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace {

using Json = nlohmann::json;

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
            asset.name = meta.value("name", entry.path().stem().string());
            asset.type = meta.value("type", "Unknown");
            asset.source = entry.path().parent_path() /
                           meta.value("source", std::string{});
            assets_.push_back(std::move(asset));
        } catch (const std::exception& exception) {
            if (log != nullptr) {
                log->write(
                    "Could not read " + entry.path().string() + ": " +
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

void TransactionStack::record(
    Guid object,
    std::string property,
    PropertyValue before,
    PropertyValue after
) {
    undo_.push_back({
        object,
        std::move(property),
        std::move(before),
        std::move(after),
    });
    redo_.clear();
}

bool TransactionStack::undo(World& world) {
    if (undo_.empty()) {
        return false;
    }
    Transaction transaction = std::move(undo_.back());
    undo_.pop_back();
    if (!apply(world, transaction, false)) {
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
    Object* object = world.findObject(transaction.object);
    if (object == nullptr || object->typeDescriptor() == nullptr) {
        return false;
    }

    for (const PropertyDescriptor* property :
         object->typeDescriptor()->allProperties()) {
        if (property->name == transaction.property && property->setter) {
            return property->setter(
                *object,
                useAfter ? transaction.after : transaction.before
            );
        }
    }
    return false;
}

} // namespace engine
