#pragma once

#include "engine/Gameplay.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

enum class ApprovalScope {
    WorldWrite,
    AssetWrite,
    CodeWrite,
    EngineCoreWrite,
    Build,
    Run,
};

[[nodiscard]] std::optional<ApprovalScope> approvalScopeFromText(
    std::string_view text
);
[[nodiscard]] std::string_view approvalScopeName(ApprovalScope scope);

struct ProjectManifest {
    int schemaVersion{1};
    Guid projectGuid;
    std::string name;
    std::filesystem::path file;
    std::filesystem::path root;
    std::filesystem::path contentRoot;
    std::filesystem::path defaultWorld;
    std::filesystem::path gameSourceRoot;
    std::string gameModuleTarget;
    std::string configurePreset;
    std::string buildPreset;
    std::string testPreset;
    std::vector<std::filesystem::path> allowedWriteRoots;

    [[nodiscard]] static std::optional<ProjectManifest> load(
        const std::filesystem::path& path,
        std::string* error = nullptr
    );
    [[nodiscard]] bool canWrite(const std::filesystem::path& path) const;
    [[nodiscard]] nlohmann::json toJson() const;
};

struct CommandResult {
    bool success{false};
    bool preview{false};
    std::string transaction;
    std::string revision;
    std::vector<Guid> changedObjects;
    std::vector<std::string> diagnostics;
    nlohmann::json output = nlohmann::json::object();

    [[nodiscard]] nlohmann::json toJson() const;
};

/// MCP, CLI, 에디터가 함께 사용하는 headless World 편집 경계다.
class AuthoringSession {
public:
    explicit AuthoringSession(ProjectManifest manifest);

    [[nodiscard]] const ProjectManifest& project() const;
    [[nodiscard]] const World& world() const;
    [[nodiscard]] World& world();
    [[nodiscard]] std::string revision() const;
    [[nodiscard]] nlohmann::json inspectWorld() const;
    [[nodiscard]] nlohmann::json queryWorld(std::string_view typeName) const;
    [[nodiscard]] nlohmann::json listTypes() const;
    [[nodiscard]] nlohmann::json describeType(std::string_view typeName) const;
    [[nodiscard]] nlohmann::json capabilities() const;

    [[nodiscard]] CommandResult validate(const nlohmann::json& script) const;
    [[nodiscard]] CommandResult preview(const nlohmann::json& script) const;
    [[nodiscard]] CommandResult apply(const nlohmann::json& script);
    bool undo();
    bool redo();
    bool save(std::string* error = nullptr) const;

private:
    [[nodiscard]] CommandResult execute(
        const nlohmann::json& script,
        World& target,
        bool commitSideEffects
    ) const;

    ProjectManifest manifest_;
    std::unique_ptr<World> world_;
    mutable OutputLog log_;
    TransactionStack transactions_;
};

} // namespace engine
