execute_process(
    COMMAND "${MCP_SERVER}" --project "${PROJECT_MANIFEST}"
    INPUT_FILE "${MCP_INPUT}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    RESULT_VARIABLE exit_code
)

if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "MCP server failed (${exit_code}): ${error_output}")
endif()

string(STRIP "${output}" output)
string(JSON protocol ERROR_VARIABLE json_error GET "${output}" result protocolVersion)
if(json_error OR NOT protocol STREQUAL "2025-11-25")
    message(FATAL_ERROR "Invalid MCP stdout JSON: ${output}")
endif()

if(NOT error_output STREQUAL "")
    message(FATAL_ERROR "MCP initialize wrote unexpected stderr: ${error_output}")
endif()
