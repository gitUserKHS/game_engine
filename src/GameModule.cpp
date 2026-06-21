#include "engine/GameModule.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Json = nlohmann::json;

template<typename Function>
Function loadFunction(void* library, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), name));
#else
    return reinterpret_cast<Function>(dlsym(library, name));
#endif
}

engine::PropertyValue valueFromJson(engine::PropertyType type, const Json& value) {
    using engine::Guid;
    using engine::PropertyType;
    switch (type) {
    case PropertyType::Boolean: return value.get<bool>();
    case PropertyType::Integer: return value.get<int>();
    case PropertyType::Float: return value.get<float>();
    case PropertyType::String: return value.get<std::string>();
    case PropertyType::Vector3:
        return glm::vec3{value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
    case PropertyType::Guid:
        return Guid::parse(value.get<std::string>()).value_or(Guid{});
    }
    return {};
}

CocoaObjectHandle toHandle(engine::Object* object) {
    return static_cast<CocoaObjectHandle>(reinterpret_cast<std::uintptr_t>(object));
}

engine::Object* fromHandle(CocoaObjectHandle handle) {
    return reinterpret_cast<engine::Object*>(static_cast<std::uintptr_t>(handle));
}

} // namespace

namespace engine {

struct GameModuleHost::Context {
    World* world{nullptr};
    InputSystem* input{nullptr};
    OutputLog* log{nullptr};
    CocoaEngineApi api{};
};

GameModuleHost::GameModuleHost() = default;
GameModuleHost::~GameModuleHost() { unload(); }

bool GameModuleHost::load(const std::filesystem::path& path, World* world,
                          InputSystem* input, OutputLog* log, std::string* error) {
    unload();
#ifdef _WIN32
    library_ = LoadLibraryW(path.c_str());
#else
    library_ = dlopen(path.c_str(), RTLD_NOW);
#endif
    if (library_ == nullptr) {
        if (error != nullptr) *error = "Could not load GameModule library.";
        return false;
    }

    using VersionFunction = uint32_t (COCOA_GAME_CALL *)(void);
    using CreateFunction = const CocoaGameModule* (COCOA_GAME_CALL *)(const CocoaEngineApi*);
    const VersionFunction version = loadFunction<VersionFunction>(library_, "CocoaGameModule_GetApiVersion");
    const CreateFunction create = loadFunction<CreateFunction>(library_, "CocoaGameModule_Create");
    if (version == nullptr || create == nullptr || version() != COCOA_GAME_MODULE_ABI_VERSION) {
        if (error != nullptr) *error = "GameModule ABI or required exports do not match.";
        unload();
        return false;
    }

    context_ = std::make_unique<Context>();
    context_->world = world;
    context_->input = input;
    context_->log = log;
    context_->api.abiVersion = COCOA_GAME_MODULE_ABI_VERSION;
    context_->api.structSize = sizeof(CocoaEngineApi);
    context_->api.context = context_.get();
    context_->api.log = [](void* raw, const char* message) {
        auto* context = static_cast<Context*>(raw);
        if (context->log != nullptr && message != nullptr) context->log->write(message);
    };
    context_->api.findActor = [](void* raw, const char* name) {
        auto* context = static_cast<Context*>(raw);
        if (context->world == nullptr || name == nullptr) return CocoaObjectHandle{0};
        for (const auto& actor : context->world->actors()) {
            if (actor->name() == name) return toHandle(actor.get());
        }
        return CocoaObjectHandle{0};
    };
    context_->api.spawnActor = [](void* raw, const char* typeName, const char* name) {
        auto* context = static_cast<Context*>(raw);
        const TypeDescriptor* type = typeName == nullptr ? nullptr : ReflectionRegistry::instance().find(typeName);
        Actor* actor = context->world == nullptr || type == nullptr ? nullptr
            : context->world->spawnActorByType(*type, name == nullptr ? typeName : name);
        return toHandle(actor);
    };
    context_->api.addComponent = [](void*, CocoaObjectHandle actorHandle,
                                    const char* typeName, const char* name) {
        auto* actor = dynamic_cast<Actor*>(fromHandle(actorHandle));
        const TypeDescriptor* type = typeName == nullptr ? nullptr : ReflectionRegistry::instance().find(typeName);
        ActorComponent* component = actor == nullptr || type == nullptr ? nullptr
            : actor->addComponentByType(*type, name == nullptr ? typeName : name);
        return toHandle(component);
    };
    context_->api.setPropertyJson = [](void*, CocoaObjectHandle objectHandle,
                                       const char* propertyName, const char* jsonValue) {
        Object* object = fromHandle(objectHandle);
        if (object == nullptr || propertyName == nullptr || jsonValue == nullptr || object->typeDescriptor() == nullptr) return 0;
        try {
            for (const PropertyDescriptor* property : object->typeDescriptor()->allProperties()) {
                if (property->name == propertyName && property->setter) {
                    return property->setter(*object, valueFromJson(property->type, Json::parse(jsonValue))) ? 1 : 0;
                }
            }
        } catch (const std::exception&) {}
        return 0;
    };
    context_->api.getInputAxis = [](void* raw, const char* name) {
        auto* context = static_cast<Context*>(raw);
        return context->input == nullptr || name == nullptr ? 0.0F : context->input->axis(name);
    };
    context_->api.raycast = [](void* raw, CocoaVec3 origin, CocoaVec3 direction,
                               float distance, CocoaRaycastHit* output) {
        auto* context = static_cast<Context*>(raw);
        if (context->world == nullptr || output == nullptr) return 0;
        const auto hit = context->world->collision().raycast(
            {origin.x, origin.y, origin.z}, {direction.x, direction.y, direction.z},
            distance, *context->world);
        if (!hit.has_value()) return 0;
        output->component = toHandle(hit->component);
        output->location = {hit->location.x, hit->location.y, hit->location.z};
        output->normal = {hit->normal.x, hit->normal.y, hit->normal.z};
        output->distance = hit->distance;
        return 1;
    };

    module_ = create(&context_->api);
    if (module_ == nullptr || module_->abiVersion != COCOA_GAME_MODULE_ABI_VERSION ||
        module_->structSize < sizeof(CocoaGameModule)) {
        if (error != nullptr) *error = "GameModule returned an invalid API table.";
        unload();
        return false;
    }
    if (module_->onLoad != nullptr) module_->onLoad(module_->userData);
    return true;
}

void GameModuleHost::unload() {
    if (module_ != nullptr) {
        if (module_->onUnload != nullptr) module_->onUnload(module_->userData);
        using DestroyFunction = void (COCOA_GAME_CALL *)(const CocoaGameModule*);
        const DestroyFunction destroy = library_ == nullptr ? nullptr
            : loadFunction<DestroyFunction>(library_, "CocoaGameModule_Destroy");
        if (destroy != nullptr) destroy(module_);
    }
    module_ = nullptr;
    context_.reset();
    if (library_ != nullptr) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(library_));
#else
        dlclose(library_);
#endif
    }
    library_ = nullptr;
}

void GameModuleHost::tick(float deltaTime) const {
    if (module_ != nullptr && module_->onTick != nullptr) module_->onTick(module_->userData, deltaTime);
}
bool GameModuleHost::loaded() const { return module_ != nullptr; }
std::string_view GameModuleHost::moduleName() const {
    return module_ == nullptr || module_->name == nullptr ? std::string_view{} : std::string_view{module_->name};
}

} // namespace engine
