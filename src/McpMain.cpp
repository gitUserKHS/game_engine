#include "engine/McpServer.hpp"

#include <iostream>
#include <set>
#include <string>

int main(int argc, char** argv) {
    std::filesystem::path projectPath = "CocoaProject.json";
    std::set<engine::ApprovalScope> scopes;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--project" && index + 1 < argc) {
            projectPath = argv[++index];
        } else if (argument == "--allow" && index + 1 < argc) {
            const auto scope = engine::approvalScopeFromText(argv[++index]);
            if (!scope.has_value()) {
                std::cerr << "Unknown approval scope.\n";
                return 2;
            }
            scopes.insert(*scope);
        } else {
            std::cerr << "Usage: cocoa_mcp_server [--project path] [--allow scope]\n";
            return 2;
        }
    }

    std::string error;
    auto manifest = engine::ProjectManifest::load(projectPath, &error);
    if (!manifest.has_value()) {
        std::cerr << error << '\n';
        return 2;
    }

    auto session = std::make_unique<engine::AuthoringSession>(std::move(*manifest));
    engine::McpServer server(std::move(session), std::move(scopes));
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto response = server.handle(nlohmann::json::parse(line));
            if (response.has_value()) {
                std::cout << response->dump() << '\n' << std::flush;
            }
        } catch (const std::exception& exception) {
            std::cerr << "MCP parse error: " << exception.what() << '\n';
        }
    }
    return 0;
}
