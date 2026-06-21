#pragma once

#include "cocoa_game_sdk/cocoa_game_sdk.h"
#include "engine/Gameplay.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace engine {

class GameModuleHost {
public:
    GameModuleHost();
    ~GameModuleHost();
    GameModuleHost(const GameModuleHost&) = delete;
    GameModuleHost& operator=(const GameModuleHost&) = delete;

    bool load(const std::filesystem::path& path, World* world, InputSystem* input,
              OutputLog* log, std::string* error = nullptr);
    void unload();
    void tick(float deltaTime) const;
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] std::string_view moduleName() const;

private:
    struct Context;
    void* library_{nullptr};
    const CocoaGameModule* module_{nullptr};
    std::unique_ptr<Context> context_;
};

} // namespace engine
