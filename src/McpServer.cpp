#include "engine/McpServer.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>

namespace {

using Json = nlohmann::json;

Json textResult(Json payload, bool isError = false) {
    return {
        {"content", Json::array({{
            {"type", "text"},
            {"text", payload.dump(2)},
        }})},
        {"structuredContent", payload},
        {"isError", isError},
    };
}

Json toolDefinition(
    std::string name,
    std::string description,
    Json properties = Json::object(),
    std::vector<std::string> required = {},
    bool readOnly = true,
    bool destructive = false
) {
    return {
        {"name", std::move(name)},
        {"description", std::move(description)},
        {"inputSchema", {
            {"type", "object"},
            {"properties", std::move(properties)},
            {"required", std::move(required)},
            {"additionalProperties", false},
        }},
        {"annotations", {
            {"readOnlyHint", readOnly},
            {"destructiveHint", destructive},
            {"idempotentHint", readOnly},
            {"openWorldHint", false},
        }},
    };
}

Json toolList() {
    const Json stringProperty{{"type", "string"}};
    const Json objectProperty{{"type", "object"}};
    return Json::array({
        toolDefinition("engine.get_capabilities", "List implemented and planned engine capabilities."),
        toolDefinition("project.describe", "Describe the active Cocoa project."),
        toolDefinition("schema.list_types", "List reflected engine types."),
        toolDefinition("schema.get_type", "Describe one reflected type.", {{"type", stringProperty}}, {"type"}),
        toolDefinition("world.inspect", "Return the complete active World JSON."),
        toolDefinition("world.query", "Query Actors by reflected type.", {{"type", stringProperty}}),
        toolDefinition("world.preview_commands", "Validate commands against a temporary World.", {{"script", objectProperty}}, {"script"}),
        toolDefinition("world.apply_commands", "Apply an approved atomic command script.", {{"script", objectProperty}}, {"script"}, false, true),
        toolDefinition("world.undo", "Undo the last authoring transaction.", {}, {}, false, true),
        toolDefinition("world.redo", "Redo the last authoring transaction.", {}, {}, false, true),
        toolDefinition("world.save", "Save the active World atomically.", {}, {}, false, true),
        toolDefinition("asset.scan", "Scan Content metadata."),
        toolDefinition("asset.import", "Import a glTF or texture asset.", {
            {"source", stringProperty}, {"kind", stringProperty}
        }, {"source"}, false, true),
        toolDefinition("script.validate", "Validate a .cocoa.json command script.", {{"script", objectProperty}}, {"script"}),
        toolDefinition("script.run", "Run an approved .cocoa.json command script.", {{"script", objectProperty}}, {"script"}, false, true),
        toolDefinition("code.scaffold_game_module", "Create the AI GameModule source template.", {{"name", stringProperty}}, {}, false, true),
        toolDefinition("build.configure", "Run the allowlisted CMake configure preset.", {}, {}, false, false),
        toolDefinition("build.compile", "Run the allowlisted CMake build preset.", {}, {}, false, false),
        toolDefinition("build.test", "Run the allowlisted CTest preset.", {}, {}, false, false),
        toolDefinition("editor.launch", "Launch the editor using the project build preset.", {}, {}, false, false),
        toolDefinition("editor.stop", "Stop an editor launched by this MCP session.", {}, {}, false, true),
        toolDefinition("editor.set_mode", "Set Edit, Simulate, or PIE through live IPC.", {{"mode", stringProperty}}, {"mode"}, false, false),
        toolDefinition("editor.capture", "Run a hidden viewport capture smoke test.", {{"path", stringProperty}}, {}, false, false),
        toolDefinition("editor.read_log", "Read the MCP editor process log."),
    });
}

bool safePreset(std::string_view value) {
    static const std::regex pattern{"^[A-Za-z0-9_-]+$"};
    return std::regex_match(value.begin(), value.end(), pattern);
}

std::string quote(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

Json runCommand(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen((command + " 2>&1").c_str(), "r");
#else
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
#endif
    if (pipe == nullptr) {
        return {{"success", false}, {"exitCode", -1}, {"output", "Could not start command."}};
    }
    std::string output;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
#ifdef _WIN32
    const int exitCode = _pclose(pipe);
#else
    const int exitCode = pclose(pipe);
#endif
    return {{"success", exitCode == 0}, {"exitCode", exitCode}, {"output", output}};
}

Json approvalError(engine::ApprovalScope scope) {
    return {
        {"success", false},
        {"error", "Approval scope is required."},
        {"requiredScope", engine::approvalScopeName(scope)},
    };
}

} // namespace

namespace engine {

McpServer::McpServer(
    std::unique_ptr<AuthoringSession> session,
    std::set<ApprovalScope> allowedScopes
)
    : session_(std::move(session)),
      allowedScopes_(std::move(allowedScopes)) {}

std::optional<Json> McpServer::handle(const Json& request) {
    if (!request.is_object() || request.value("jsonrpc", "") != "2.0" ||
        !request.contains("method")) {
        return Json{{"jsonrpc", "2.0"}, {"id", nullptr}, {"error", {
            {"code", -32600}, {"message", "Invalid Request"}
        }}};
    }
    if (!request.contains("id")) {
        return std::nullopt;
    }

    const Json id = request.at("id");
    const std::string method = request.at("method").get<std::string>();
    try {
        Json result;
        if (method == "initialize") {
            result = {
                {"protocolVersion", "2025-11-25"},
                {"capabilities", {
                    {"tools", { {"listChanged", false} }},
                    {"resources", { {"subscribe", false}, {"listChanged", false} }},
                }},
                {"serverInfo", {{"name", "cocoa-mcp-server"}, {"version", "0.1.0"}}},
                {"instructions", "Inspect capabilities and schema before previewing atomic World commands."},
            };
        } else if (method == "ping") {
            result = Json::object();
        } else if (method == "tools/list") {
            result = {{"tools", toolList()}};
        } else if (method == "tools/call") {
            const Json& parameters = request.value("params", Json::object());
            result = callTool(
                parameters.value("name", ""),
                parameters.value("arguments", Json::object())
            );
        } else if (method == "resources/list") {
            result = {{"resources", Json::array({
                {{"uri", "cocoa://project"}, {"name", "Project manifest"}, {"mimeType", "application/json"}},
                {{"uri", "cocoa://world"}, {"name", "Active World"}, {"mimeType", "application/json"}},
                {{"uri", "cocoa://capabilities"}, {"name", "Engine capabilities"}, {"mimeType", "application/json"}}
            })}};
        } else if (method == "resources/read") {
            const std::string uri = request.at("params").value("uri", "");
            Json payload;
            if (uri == "cocoa://project") {
                payload = session_->project().toJson();
            } else if (uri == "cocoa://world") {
                payload = session_->inspectWorld();
            } else if (uri == "cocoa://capabilities") {
                payload = session_->capabilities();
            } else {
                throw std::runtime_error("Unknown resource URI.");
            }
            result = {{"contents", Json::array({{
                {"uri", uri}, {"mimeType", "application/json"}, {"text", payload.dump(2)}
            }})}};
        } else {
            return Json{{"jsonrpc", "2.0"}, {"id", id}, {"error", {
                {"code", -32601}, {"message", "Method not found"}
            }}};
        }
        return Json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
    } catch (const std::exception& exception) {
        return Json{{"jsonrpc", "2.0"}, {"id", id}, {"error", {
            {"code", -32603}, {"message", exception.what()}
        }}};
    }
}

bool McpServer::allowed(ApprovalScope scope) const {
    return allowedScopes_.contains(scope);
}

Json McpServer::callTool(std::string_view name, const Json& arguments) {
    if (name == "engine.get_capabilities") {
        return textResult(session_->capabilities());
    }
    if (name == "project.describe") {
        return textResult(session_->project().toJson());
    }
    if (name == "schema.list_types") {
        return textResult(session_->listTypes());
    }
    if (name == "schema.get_type") {
        return textResult(session_->describeType(arguments.at("type").get<std::string>()));
    }
    if (name == "world.inspect") {
        return textResult(session_->inspectWorld());
    }
    if (name == "world.query") {
        return textResult(session_->queryWorld(arguments.value("type", "")));
    }
    if (name == "world.preview_commands" || name == "script.validate") {
        return textResult(session_->preview(arguments.at("script")).toJson());
    }
    if (name == "world.apply_commands" || name == "script.run") {
        if (!allowed(ApprovalScope::WorldWrite)) {
            return textResult(approvalError(ApprovalScope::WorldWrite), true);
        }
        const CommandResult result = session_->apply(arguments.at("script"));
        return textResult(result.toJson(), !result.success);
    }
    if (name == "world.undo" || name == "world.redo") {
        if (!allowed(ApprovalScope::WorldWrite)) {
            return textResult(approvalError(ApprovalScope::WorldWrite), true);
        }
        const bool success = name == "world.undo" ? session_->undo() : session_->redo();
        return textResult({{"success", success}, {"revision", session_->revision()}}, !success);
    }
    if (name == "world.save") {
        if (!allowed(ApprovalScope::WorldWrite)) {
            return textResult(approvalError(ApprovalScope::WorldWrite), true);
        }
        std::string error;
        const bool success = session_->save(&error);
        return textResult({{"success", success}, {"error", error}}, !success);
    }
    if (name == "asset.scan") {
        AssetRegistry registry;
        OutputLog log;
        registry.scan(session_->project().contentRoot, &log);
        Json assets = Json::array();
        for (const AssetData& asset : registry.assets()) {
            assets.push_back({
                {"guid", asset.guid.toString()}, {"name", asset.name},
                {"type", asset.type}, {"source", asset.source.string()}
            });
        }
        return textResult({{"assets", std::move(assets)}});
    }
    if (name == "asset.import") {
        if (!allowed(ApprovalScope::AssetWrite)) {
            return textResult(approvalError(ApprovalScope::AssetWrite), true);
        }
        Json script{
            {"schemaVersion", 1}, {"transaction", "MCP asset import"},
            {"commands", Json::array({{
                {"op", "import_asset"}, {"source", arguments.at("source")},
                {"kind", arguments.value("kind", "gltf")}
            }})}
        };
        const CommandResult result = session_->apply(script);
        return textResult(result.toJson(), !result.success);
    }
    if (name == "code.scaffold_game_module") {
        if (!allowed(ApprovalScope::CodeWrite)) {
            return textResult(approvalError(ApprovalScope::CodeWrite), true);
        }
        const auto& project = session_->project();
        const std::filesystem::path output = project.gameSourceRoot / "AIGameModule.cpp";
        if (!project.canWrite(output)) {
            return textResult({{"success", false}, {"error", "Game source path is not allowed."}}, true);
        }
        std::filesystem::create_directories(output.parent_path());
        if (!std::filesystem::exists(output)) {
            const std::filesystem::path sample =
                project.root / "Game/Source/SampleGameModule.cpp";
            std::error_code copyError;
            std::filesystem::copy_file(sample, output, copyError);
            if (copyError) {
                return textResult({
                    {"success", false},
                    {"error", "Could not copy the GameModule template."}
                }, true);
            }
        }
        return textResult({
            {"success", true},
            {"path", output.string()},
            {"target", "ai_game_module"},
            {"next", "Run build.configure before build.compile."}
        });
    }
    if (name == "build.configure" || name == "build.compile" || name == "build.test") {
        if (!allowed(ApprovalScope::Build)) {
            return textResult(approvalError(ApprovalScope::Build), true);
        }
        const auto& project = session_->project();
        std::string command;
        if (name == "build.configure" && safePreset(project.configurePreset)) {
            command = "cmake --preset " + project.configurePreset;
        } else if (name == "build.compile" && safePreset(project.buildPreset)) {
            command = "cmake --build --preset " + project.buildPreset;
        } else if (name == "build.test" && safePreset(project.testPreset)) {
            command = "ctest --preset " + project.testPreset + " --output-on-failure";
        } else {
            return textResult({{"success", false}, {"error", "Unsafe build preset."}}, true);
        }
        const std::filesystem::path old = std::filesystem::current_path();
        std::filesystem::current_path(project.root);
        Json result = runCommand(command);
        std::filesystem::current_path(old);
        return textResult(result, !result.value("success", false));
    }
    if (name == "editor.capture") {
        if (!allowed(ApprovalScope::Run)) {
            return textResult(approvalError(ApprovalScope::Run), true);
        }
        const auto& project = session_->project();
        const std::filesystem::path capturePath = project.root /
            arguments.value("path", "Saved/Agent/mcp-capture.png");
        if (!project.canWrite(capturePath)) {
            return textResult({{"success", false}, {"error", "Capture path is not allowed."}}, true);
        }
        const std::filesystem::path executable =
            project.root / "out/build" / project.buildPreset / "topdown_engine.exe";
        const std::string command = quote(executable) + " --hidden --capture " +
            quote(capturePath) +
            " --capture-target both --capture-frame 3 --exit-after-capture";
        Json result = runCommand(command);
        result["capturePath"] = capturePath.string();
        return textResult(result, !result.value("success", false));
    }
    if (name == "editor.read_log") {
        const std::filesystem::path path =
            session_->project().root / "Saved/Agent/editor.log";
        std::ifstream stream(path);
        std::ostringstream text;
        text << stream.rdbuf();
        return textResult({{"path", path.string()}, {"text", text.str()}});
    }
    if (name == "editor.launch" || name == "editor.stop" || name == "editor.set_mode") {
        return textResult({
            {"success", false},
            {"error", "Live editor named-pipe control is not available in this build."},
            {"capability", "live-named-pipe"}
        }, true);
    }
    return textResult({{"success", false}, {"error", "Unknown tool."}}, true);
}

} // namespace engine
