#include "cocoa_game_sdk/cocoa_game_sdk.h"

namespace {
struct SampleState { const CocoaEngineApi* engine{nullptr}; float elapsed{0.0F}; };
SampleState state;
void COCOA_GAME_CALL onLoad(void* value) {
    auto* sample = static_cast<SampleState*>(value);
    sample->engine->log(sample->engine->context, "Sample GameModule loaded.");
}
void COCOA_GAME_CALL onUnload(void* value) {
    auto* sample = static_cast<SampleState*>(value);
    sample->engine->log(sample->engine->context, "Sample GameModule unloaded.");
}
void COCOA_GAME_CALL onTick(void* value, float deltaTime) {
    static_cast<SampleState*>(value)->elapsed += deltaTime;
}
CocoaGameModule module{COCOA_GAME_MODULE_ABI_VERSION, sizeof(CocoaGameModule),
                       "SampleGameModule", &state, &onLoad, &onUnload, &onTick};
}

extern "C" COCOA_GAME_EXPORT uint32_t COCOA_GAME_CALL CocoaGameModule_GetApiVersion(void) {
    return COCOA_GAME_MODULE_ABI_VERSION;
}
extern "C" COCOA_GAME_EXPORT const CocoaGameModule* COCOA_GAME_CALL
CocoaGameModule_Create(const CocoaEngineApi* engineApi) {
    if (engineApi == nullptr || engineApi->abiVersion != COCOA_GAME_MODULE_ABI_VERSION ||
        engineApi->structSize < sizeof(CocoaEngineApi)) return nullptr;
    state.engine = engineApi;
    state.elapsed = 0.0F;
    return &module;
}
extern "C" COCOA_GAME_EXPORT void COCOA_GAME_CALL
CocoaGameModule_Destroy(const CocoaGameModule*) { state.engine = nullptr; }
