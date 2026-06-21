#pragma once

#include <stdint.h>

#ifdef _WIN32
#define COCOA_GAME_EXPORT __declspec(dllexport)
#define COCOA_GAME_CALL __cdecl
#else
#define COCOA_GAME_EXPORT __attribute__((visibility("default")))
#define COCOA_GAME_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define COCOA_GAME_MODULE_ABI_VERSION 1u

typedef uint64_t CocoaObjectHandle;

typedef struct CocoaVec3 {
    float x;
    float y;
    float z;
} CocoaVec3;

typedef struct CocoaRaycastHit {
    CocoaObjectHandle component;
    CocoaVec3 location;
    CocoaVec3 normal;
    float distance;
} CocoaRaycastHit;

typedef struct CocoaEngineApi {
    uint32_t abiVersion;
    uint32_t structSize;
    void* context;
    void (COCOA_GAME_CALL *log)(void* context, const char* message);
    CocoaObjectHandle (COCOA_GAME_CALL *findActor)(void* context, const char* name);
    CocoaObjectHandle (COCOA_GAME_CALL *spawnActor)(void* context, const char* typeName, const char* name);
    CocoaObjectHandle (COCOA_GAME_CALL *addComponent)(void* context, CocoaObjectHandle actor, const char* typeName, const char* name);
    int (COCOA_GAME_CALL *setPropertyJson)(void* context, CocoaObjectHandle object, const char* property, const char* jsonValue);
    float (COCOA_GAME_CALL *getInputAxis)(void* context, const char* axisName);
    int (COCOA_GAME_CALL *raycast)(void* context, CocoaVec3 origin, CocoaVec3 direction, float distance, CocoaRaycastHit* hit);
} CocoaEngineApi;

typedef struct CocoaGameModule {
    uint32_t abiVersion;
    uint32_t structSize;
    const char* name;
    void* userData;
    void (COCOA_GAME_CALL *onLoad)(void* userData);
    void (COCOA_GAME_CALL *onUnload)(void* userData);
    void (COCOA_GAME_CALL *onTick)(void* userData, float deltaTime);
} CocoaGameModule;

COCOA_GAME_EXPORT uint32_t COCOA_GAME_CALL CocoaGameModule_GetApiVersion(void);
COCOA_GAME_EXPORT const CocoaGameModule* COCOA_GAME_CALL CocoaGameModule_Create(const CocoaEngineApi* engineApi);
COCOA_GAME_EXPORT void COCOA_GAME_CALL CocoaGameModule_Destroy(const CocoaGameModule* module);

#ifdef __cplusplus
}
#endif
