#pragma once

#include "engine/Authoring.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <set>

namespace engine {

class McpServer {
public:
    McpServer(
        std::unique_ptr<AuthoringSession> session,
        std::set<ApprovalScope> allowedScopes = {}
    );

    [[nodiscard]] std::optional<nlohmann::json> handle(
        const nlohmann::json& request
    );

private:
    [[nodiscard]] nlohmann::json callTool(
        std::string_view name,
        const nlohmann::json& arguments
    );
    [[nodiscard]] bool allowed(ApprovalScope scope) const;

    std::unique_ptr<AuthoringSession> session_;
    std::set<ApprovalScope> allowedScopes_;
};

} // namespace engine
