file(REMOVE_RECURSE "${PACKAGE_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PACKAGE_DIR}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Package install failed:\n${install_output}\n${install_error}")
endif()

set(required_files
    topdown_engine.exe
    cocoa_mcp_server.exe
    CocoaProject.json
    CMakeLists.txt
    CMakePresets.json
    shaders/basic.vert
    Content/Input/default.input.json
    include/cocoa_game_sdk/cocoa_game_sdk.h
    lib/cmake/CocoaGameSDK/CocoaGameSDKConfig.cmake
    Game/Source/SampleGameModule.cpp
    examples/BlockWorld.cocoa.json
    run-editor.cmd
)
foreach(path IN LISTS required_files)
    if(NOT EXISTS "${PACKAGE_DIR}/${path}")
        message(FATAL_ERROR "Required package file is missing: ${path}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --preset windows-debug
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-DCMAKE_RC_COMPILER=${RC_COMPILER}"
        "-DCMAKE_MT=${MT_TOOL}"
    WORKING_DIRECTORY "${PACKAGE_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Packaged SDK configure failed:\n${configure_output}\n${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build --preset windows-debug
    WORKING_DIRECTORY "${PACKAGE_DIR}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Packaged GameModule build failed:\n${build_output}\n${build_error}")
endif()

if(NOT EXISTS "${PACKAGE_DIR}/Game/Binaries/sample_game_module.dll")
    message(FATAL_ERROR "Packaged GameModule DLL was not created.")
endif()

execute_process(
    COMMAND "${PACKAGE_DIR}/cocoa_mcp_server.exe"
        --project "${PACKAGE_DIR}/CocoaProject.json"
    INPUT_FILE "${SOURCE_DIR}/tests/mcp_initialize.jsonl"
    RESULT_VARIABLE mcp_result
    OUTPUT_VARIABLE mcp_output
    ERROR_VARIABLE mcp_error
)
if(NOT mcp_result EQUAL 0 OR NOT mcp_output MATCHES "2025-11-25")
    message(FATAL_ERROR "Packaged MCP smoke failed:\n${mcp_output}\n${mcp_error}")
endif()
