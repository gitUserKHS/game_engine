#include "engine/Authoring.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

using Json = nlohmann::json;

std::string pathText(const std::filesystem::path& path) {
    const std::u8string value = path.generic_u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::filesystem::path normalizedPath(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        return path.lexically_normal();
    }
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(absolute, error);
    return error ? absolute.lexically_normal() : canonical;
}

bool isInside(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root
) {
    const std::filesystem::path relative =
        normalizedPath(candidate).lexically_relative(normalizedPath(root));
    if (relative.empty()) {
        return true;
    }
    for (const auto& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return !relative.is_absolute();
}

std::string propertyTypeName(engine::PropertyType type) {
    using engine::PropertyType;
    switch (type) {
    case PropertyType::Boolean:
        return "boolean";
    case PropertyType::Integer:
        return "integer";
    case PropertyType::Float:
        return "number";
    case PropertyType::String:
        return "string";
    case PropertyType::Vector3:
        return "vector3";
    case PropertyType::Guid:
        return "guid";
    }
    return "unknown";
}

engine::PropertyValue propertyValueFromJson(
    engine::PropertyType type,
    const Json& value
) {
    using engine::Guid;
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
        if (!value.is_array() || value.size() != 3) {
            throw std::runtime_error("Vector3 requires [x, y, z].");
        }
        return glm::vec3{
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
        };
    case PropertyType::Guid:
        return Guid::parse(value.get<std::string>()).value_or(Guid{});
    }
    return {};
}

std::string revisionOf(const engine::World& world) {
    const std::string text = engine::WorldSerializer::toJson(world);
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
}

bool atomicSaveWorld(
    const engine::World& world,
    const std::filesystem::path& path
) {
    std::filesystem::path temporary = path;
    temporary += L".tmp";
    if (!engine::WorldSerializer::save(world, temporary)) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace

namespace engine {

std::optional<ApprovalScope> approvalScopeFromText(std::string_view text) {
    static constexpr std::pair<std::string_view, ApprovalScope> values[]{
        {"world-write", ApprovalScope::WorldWrite},
        {"asset-write", ApprovalScope::AssetWrite},
        {"code-write", ApprovalScope::CodeWrite},
        {"engine-core-write", ApprovalScope::EngineCoreWrite},
        {"build", ApprovalScope::Build},
        {"run", ApprovalScope::Run},
    };
    for (const auto& [name, scope] : values) {
        if (name == text) {
            return scope;
        }
    }
    return std::nullopt;
}

std::string_view approvalScopeName(ApprovalScope scope) {
    switch (scope) {
    case ApprovalScope::WorldWrite:
        return "world-write";
    case ApprovalScope::AssetWrite:
        return "asset-write";
    case ApprovalScope::CodeWrite:
        return "code-write";
    case ApprovalScope::EngineCoreWrite:
        return "engine-core-write";
    case ApprovalScope::Build:
        return "build";
    case ApprovalScope::Run:
        return "run";
    }
    return "unknown";
}

std::optional<ProjectManifest> ProjectManifest::load(
    const std::filesystem::path& path,
    std::string* error
) {
    try {
        std::ifstream stream(path);
        if (!stream) {
            throw std::runtime_error("Could not open project manifest.");
        }
        Json json;
        stream >> json;

        ProjectManifest result;
        result.file = normalizedPath(path);
        result.root = result.file.parent_path();
        result.schemaVersion = json.value("schemaVersion", 0);
        result.projectGuid = Guid::parse(json.value("projectGuid", ""))
                                 .value_or(Guid{});
        result.name = json.value("name", "CocoaProject");
        result.contentRoot = result.root / json.value("contentRoot", "Content");
        result.defaultWorld =
            result.root / json.value("defaultWorld", "Content/World.world.json");
        result.gameSourceRoot =
            result.root / json.value("gameSourceRoot", "Game/Source");
        result.gameModuleTarget = json.value("gameModuleTarget", "game_module");
        result.editorExecutable = result.root / json.value(
            "editorExecutable", "out/build/windows-debug/topdown_engine.exe"
        );
        result.configurePreset = json.value("configurePreset", "windows-debug");
        result.buildPreset = json.value("buildPreset", "windows-debug");
        result.testPreset = json.value("testPreset", "windows-debug");
        for (const std::string& item :
             json.value("allowedWriteRoots", std::vector<std::string>{})) {
            result.allowedWriteRoots.push_back(result.root / item);
        }
        if (result.schemaVersion != 1 || !result.projectGuid.valid()) {
            throw std::runtime_error("Unsupported manifest or invalid project GUID.");
        }
        return result;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return std::nullopt;
    }
}

bool ProjectManifest::canWrite(const std::filesystem::path& path) const {
    return std::any_of(
        allowedWriteRoots.begin(),
        allowedWriteRoots.end(),
        [&path](const std::filesystem::path& rootPath) {
            return isInside(path, rootPath);
        }
    );
}

Json ProjectManifest::toJson() const {
    Json roots = Json::array();
    for (const auto& path : allowedWriteRoots) {
        roots.push_back(pathText(path));
    }
    return {
        {"schemaVersion", schemaVersion},
        {"projectGuid", projectGuid.toString()},
        {"name", name},
        {"root", pathText(root)},
        {"contentRoot", pathText(contentRoot)},
        {"defaultWorld", pathText(defaultWorld)},
        {"gameSourceRoot", pathText(gameSourceRoot)},
        {"gameModuleTarget", gameModuleTarget},
        {"editorExecutable", pathText(editorExecutable)},
        {"configurePreset", configurePreset},
        {"buildPreset", buildPreset},
        {"testPreset", testPreset},
        {"allowedWriteRoots", std::move(roots)},
    };
}

Json CommandResult::toJson() const {
    Json changed = Json::array();
    for (Guid guid : changedObjects) {
        changed.push_back(guid.toString());
    }
    return {
        {"success", success},
        {"preview", preview},
        {"transaction", transaction},
        {"revision", revision},
        {"changedObjects", std::move(changed)},
        {"diagnostics", diagnostics},
        {"output", output},
    };
}

AuthoringSession::AuthoringSession(ProjectManifest manifest)
    : manifest_(std::move(manifest)) {
    registerEngineTypes();
    world_ = WorldSerializer::load(manifest_.defaultWorld, &log_);
    if (world_ == nullptr) {
        world_ = std::make_unique<World>("AgentWorld");
    }
}

const ProjectManifest& AuthoringSession::project() const {
    return manifest_;
}

const World& AuthoringSession::world() const {
    return *world_;
}

World& AuthoringSession::world() {
    return *world_;
}

std::string AuthoringSession::revision() const {
    return revisionOf(*world_);
}

Json AuthoringSession::inspectWorld() const {
    return Json::parse(WorldSerializer::toJson(*world_));
}

Json AuthoringSession::queryWorld(std::string_view typeName) const {
    Json result = Json::array();
    for (const auto& actor : world_->actors()) {
        if (!typeName.empty() && actor->typeName() != typeName) {
            continue;
        }
        result.push_back({
            {"guid", actor->guid().toString()},
            {"name", actor->name()},
            {"type", actor->typeName()},
            {"componentCount", actor->components().size()},
        });
    }
    return result;
}

Json AuthoringSession::listTypes() const {
    Json result = Json::array();
    for (const auto& type : ReflectionRegistry::instance().types()) {
        result.push_back(type->name);
    }
    return result;
}

Json AuthoringSession::describeType(std::string_view typeName) const {
    const TypeDescriptor* type = ReflectionRegistry::instance().find(typeName);
    if (type == nullptr) {
        return {{"error", "Unknown type."}};
    }
    Json properties = Json::array();
    for (const PropertyDescriptor* property : type->allProperties()) {
        properties.push_back({
            {"name", property->name},
            {"category", property->category},
            {"type", propertyTypeName(property->type)},
            {"editable", hasFlag(property->flags, PropertyFlags::Editable)},
            {"serializable", hasFlag(property->flags, PropertyFlags::Serializable)},
        });
    }
    return {
        {"name", type->name},
        {"parent", type->parent == nullptr ? "" : type->parent->name},
        {"properties", std::move(properties)},
    };
}

Json AuthoringSession::capabilities() const {
    return {
        {"authoringCommandSchema", 1},
        {"mcpProtocol", "2025-11-25"},
        {"offlineWorldEditing", true},
        {"liveEditorEditing", false},
        {"gameModuleAbi", 1},
        {"implemented", Json::array({
            "actor-component-authoring",
            "reflection-schema",
            "world-save-load",
            "asset-import",
            "build-test",
            "screenshot-capture"
        })},
        {"planned", Json::array({
            "live-named-pipe",
            "voxel-world",
            "world-partition",
            "network-replication"
        })},
    };
}

CommandResult AuthoringSession::validate(const Json& script) const {
    CommandResult result = preview(script);
    result.output = Json::object();
    return result;
}

CommandResult AuthoringSession::preview(const Json& script) const {
    auto copy = WorldSerializer::fromJson(WorldSerializer::toJson(*world_), &log_);
    CommandResult result = execute(script, *copy, false);
    result.preview = true;
    if (result.success) {
        result.output["world"] = Json::parse(WorldSerializer::toJson(*copy));
    }
    return result;
}

CommandResult AuthoringSession::apply(const Json& script) {
    CommandResult checked = preview(script);
    if (!checked.success) {
        return checked;
    }

    const std::string before = WorldSerializer::toJson(*world_);
    CommandResult result = execute(script, *world_, true);
    if (!result.success) {
        WorldSerializer::restore(*world_, before, &log_);
        return result;
    }
    const std::string after = WorldSerializer::toJson(*world_);
    transactions_.recordSnapshot(before, after);
    result.revision = revision();
    return result;
}

bool AuthoringSession::undo() {
    return transactions_.undo(*world_);
}

bool AuthoringSession::redo() {
    return transactions_.redo(*world_);
}

bool AuthoringSession::save(std::string* error) const {
    if (!manifest_.canWrite(manifest_.defaultWorld)) {
        if (error != nullptr) {
            *error = "Default World is outside allowed write roots.";
        }
        return false;
    }
    if (!atomicSaveWorld(*world_, manifest_.defaultWorld)) {
        if (error != nullptr) {
            *error = "Could not atomically save the World.";
        }
        return false;
    }
    return true;
}

CommandResult AuthoringSession::execute(
    const Json& script,
    World& target,
    bool commitSideEffects
) const {
    CommandResult result;
    result.preview = !commitSideEffects;
    result.transaction = script.value("transaction", "AI Authoring");
    result.revision = revisionOf(target);

    try {
        if (!script.is_object() || script.value("schemaVersion", 0) != 1 ||
            !script.contains("commands") || !script.at("commands").is_array()) {
            throw std::runtime_error("Invalid Cocoa command script schema.");
        }
        if (script.contains("baseWorldRevision") &&
            script.at("baseWorldRevision").get<std::string>() != result.revision) {
            throw std::runtime_error("Stale World revision.");
        }

        std::unordered_map<std::string, Guid> aliases;
        std::vector<std::filesystem::path> savePaths;
        const auto resolve = [&](const Json& value) -> Object* {
            const std::string text = value.get<std::string>();
            const auto alias = aliases.find(text);
            const Guid guid = alias == aliases.end()
                                  ? Guid::parse(text).value_or(Guid{})
                                  : alias->second;
            return target.findObject(guid);
        };
        const auto remember = [&](const Json& command, Object& object) {
            if (command.contains("alias")) {
                aliases[command.at("alias").get<std::string>()] = object.guid();
            }
            result.changedObjects.push_back(object.guid());
        };

        for (const Json& command : script.at("commands")) {
            const std::string operation = command.value("op", "");
            if (operation == "spawn_actor") {
                const std::string typeName = command.value("type", "Actor");
                const TypeDescriptor* type = ReflectionRegistry::instance().find(typeName);
                Actor* actor = type == nullptr
                                   ? nullptr
                                   : target.spawnActorByType(
                                         *type,
                                         command.value("name", typeName)
                                     );
                if (actor == nullptr) {
                    throw std::runtime_error("Could not spawn Actor type '" + typeName + "'.");
                }
                remember(command, *actor);
            } else if (operation == "add_component") {
                auto* actor = dynamic_cast<Actor*>(resolve(command.at("actor")));
                const std::string typeName = command.at("type").get<std::string>();
                const TypeDescriptor* type = ReflectionRegistry::instance().find(typeName);
                ActorComponent* component = actor == nullptr || type == nullptr
                                                ? nullptr
                                                : actor->addComponentByType(
                                                      *type,
                                                      command.value("name", typeName)
                                                  );
                if (component == nullptr) {
                    throw std::runtime_error("Could not add component '" + typeName + "'.");
                }
                remember(command, *component);
            } else if (operation == "set_property") {
                Object* object = resolve(command.at("target"));
                if (object == nullptr || object->typeDescriptor() == nullptr) {
                    throw std::runtime_error("Property target was not found.");
                }
                const std::string name = command.at("property").get<std::string>();
                const PropertyDescriptor* descriptor = nullptr;
                for (const PropertyDescriptor* property :
                     object->typeDescriptor()->allProperties()) {
                    if (property->name == name) {
                        descriptor = property;
                        break;
                    }
                }
                if (descriptor == nullptr || !descriptor->setter ||
                    !hasFlag(descriptor->flags, PropertyFlags::Editable) ||
                    !descriptor->setter(
                        *object,
                        propertyValueFromJson(descriptor->type, command.at("value"))
                    )) {
                    throw std::runtime_error("Property '" + name + "' is not editable.");
                }
                result.changedObjects.push_back(object->guid());
            } else if (operation == "attach") {
                auto* child = dynamic_cast<SceneComponent*>(resolve(command.at("child")));
                auto* parent = dynamic_cast<SceneComponent*>(resolve(command.at("parent")));
                if (child == nullptr || parent == nullptr || !child->attachTo(parent)) {
                    throw std::runtime_error("Could not attach SceneComponents.");
                }
                result.changedObjects.push_back(child->guid());
            } else if (operation == "set_root") {
                auto* actor = dynamic_cast<Actor*>(resolve(command.at("actor")));
                auto* component = dynamic_cast<SceneComponent*>(resolve(command.at("component")));
                if (actor == nullptr || component == nullptr || component->owner() != actor) {
                    throw std::runtime_error("Could not set Actor root component.");
                }
                actor->setRootComponent(component);
                result.changedObjects.push_back(actor->guid());
            } else if (operation == "possess") {
                auto* controller = dynamic_cast<Controller*>(resolve(command.at("controller")));
                auto* pawn = dynamic_cast<Pawn*>(resolve(command.at("pawn")));
                if (controller == nullptr || pawn == nullptr ||
                    controller->world() != pawn->world()) {
                    throw std::runtime_error("Could not possess Pawn.");
                }
                controller->possess(pawn);
                result.changedObjects.push_back(controller->guid());
            } else if (operation == "duplicate_actor") {
                auto* source = dynamic_cast<Actor*>(resolve(command.at("target")));
                Actor* duplicate = source == nullptr
                                       ? nullptr
                                       : WorldSerializer::duplicateActor(
                                             target,
                                             *source,
                                             command.value("name", source->name() + " Copy"),
                                             &log_
                                         );
                if (duplicate == nullptr) {
                    throw std::runtime_error("Could not duplicate Actor.");
                }
                remember(command, *duplicate);
            } else if (operation == "destroy_actor") {
                auto* actor = dynamic_cast<Actor*>(resolve(command.at("target")));
                if (actor == nullptr) {
                    throw std::runtime_error("Actor to destroy was not found.");
                }
                result.changedObjects.push_back(actor->guid());
                target.destroyActor(*actor);
            } else if (operation == "import_asset") {
                const std::filesystem::path source = normalizedPath(
                    manifest_.root / command.at("source").get<std::string>()
                );
                if (!std::filesystem::exists(source)) {
                    throw std::runtime_error("Asset import source does not exist.");
                }
                if (commitSideEffects) {
                    const std::string kind = command.value("kind", "gltf");
                    const AssetImportResult imported = kind == "texture"
                        ? AssetImporter::importTexture(source, manifest_.contentRoot, &log_)
                        : AssetImporter::importGltfAsStaticMesh(source, manifest_.contentRoot, &log_);
                    if (!imported.success) {
                        throw std::runtime_error(imported.error);
                    }
                    result.output["importedAssets"].push_back(imported.asset.guid.toString());
                }
            } else if (operation == "save_world") {
                const std::filesystem::path path = command.contains("path")
                    ? manifest_.root / command.at("path").get<std::string>()
                    : manifest_.defaultWorld;
                if (!manifest_.canWrite(path)) {
                    throw std::runtime_error("World save path is outside allowed roots.");
                }
                savePaths.push_back(path);
            } else {
                throw std::runtime_error("Unknown authoring operation '" + operation + "'.");
            }
        }

        if (commitSideEffects) {
            for (const auto& path : savePaths) {
                if (!atomicSaveWorld(target, path)) {
                    throw std::runtime_error("Could not save World to " + pathText(path));
                }
            }
        }

        std::sort(result.changedObjects.begin(), result.changedObjects.end());
        result.changedObjects.erase(
            std::unique(result.changedObjects.begin(), result.changedObjects.end()),
            result.changedObjects.end()
        );
        Json aliasOutput = Json::object();
        for (const auto& [name, guid] : aliases) {
            aliasOutput[name] = guid.toString();
        }
        result.output["aliases"] = std::move(aliasOutput);
        result.success = true;
        result.revision = revisionOf(target);
    } catch (const std::exception& exception) {
        result.success = false;
        result.diagnostics.push_back(exception.what());
    }
    return result;
}

} // namespace engine
