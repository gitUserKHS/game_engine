cmake_minimum_required(VERSION 3.20)

project(CocoaLearningEngine VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:/utf-8>"
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
    )
endif()

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9-docking
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG 1.83
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glm imgui nlohmann_json imguizmo stb)

find_package(OpenGL REQUIRED)

add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_library(engine_core STATIC
    src/Core.cpp
    src/World.cpp
    src/Components.cpp
    src/Systems.cpp
    src/Gameplay.cpp
    src/Reflection.cpp
    src/Editor.cpp
    src/Screenshot.cpp
)
target_include_directories(engine_core PUBLIC include PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(engine_core PUBLIC glm::glm nlohmann_json::nlohmann_json)
target_compile_definitions(engine_core PRIVATE GLM_ENABLE_EXPERIMENTAL)

add_library(cocoa_game_sdk INTERFACE)
target_include_directories(
    cocoa_game_sdk
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_library(engine_authoring STATIC
    src/Authoring.cpp
    src/McpServer.cpp
    src/GameModule.cpp
)
target_include_directories(engine_authoring PUBLIC include)
target_link_libraries(
    engine_authoring
    PUBLIC engine_core cocoa_game_sdk nlohmann_json::nlohmann_json
)

add_library(sample_game_module SHARED Game/Source/SampleGameModule.cpp)
target_link_libraries(sample_game_module PRIVATE cocoa_game_sdk)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Source/AIGameModule.cpp")
    add_library(ai_game_module SHARED Game/Source/AIGameModule.cpp)
    target_link_libraries(ai_game_module PRIVATE cocoa_game_sdk)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Source/AIGameModule.cpp")
    add_library(ai_game_module SHARED Game/Source/AIGameModule.cpp)
    target_link_libraries(ai_game_module PRIVATE cocoa_game_sdk)
endif()

add_executable(cocoa_mcp_server src/McpMain.cpp)
target_link_libraries(cocoa_mcp_server PRIVATE engine_authoring)

add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)

target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imguizmo_SOURCE_DIR}
)

target_compile_definitions(
    imgui_backend
    PRIVATE
        IMGUI_IMPL_OPENGL_LOADER_GLAD2
        IMGUI_DEFINE_MATH_OPERATORS
)
target_link_libraries(imgui_backend PUBLIC glfw glad OpenGL::GL)

add_executable(topdown_engine
    src/main.cpp
    src/Application.cpp
    src/Renderer.cpp
)

target_include_directories(topdown_engine PRIVATE include ${stb_SOURCE_DIR})
target_compile_definitions(
    topdown_engine
    PRIVATE ENGINE_SHADER_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            ENGINE_CONTENT_DIR=L"${CMAKE_CURRENT_SOURCE_DIR}/Content"
)
target_link_libraries(
    topdown_engine
    PRIVATE
        glfw
        glad
        engine_core
        imgui_backend
        OpenGL::GL
)

if(MSVC)
    target_compile_options(topdown_engine PRIVATE /W4 /permissive-)
    target_compile_options(engine_core PRIVATE /W4 /permissive-)
    target_compile_options(engine_authoring PRIVATE /W4 /permissive-)
    target_compile_options(cocoa_mcp_server PRIVATE /W4 /permissive-)
    target_compile_options(sample_game_module PRIVATE /W4 /permissive-)
    if(TARGET ai_game_module)
        target_compile_options(ai_game_module PRIVATE /W4 /permissive-)
    endif()
    if(TARGET ai_game_module)
        target_compile_options(ai_game_module PRIVATE /W4 /permissive-)
    endif()
    target_compile_options(
        imgui_backend
        PRIVATE /FI${CMAKE_CURRENT_SOURCE_DIR}/third_party/imguizmo_compat.hpp
    )
else()
    target_compile_options(topdown_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(engine_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(engine_authoring PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cocoa_mcp_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(sample_game_module PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET ai_game_module)
        target_compile_options(ai_game_module PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET ai_game_module)
        target_compile_options(ai_game_module PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_options(
        imgui_backend
        PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imguizmo_compat.hpp
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/EngineTests.cpp)
    target_include_directories(engine_tests PRIVATE ${stb_SOURCE_DIR})
    target_link_libraries(engine_tests PRIVATE engine_core engine_authoring)
    add_dependencies(engine_tests sample_game_module)
    add_test(NAME engine_tests COMMAND engine_tests)
    add_test(
        NAME mcp_stdio_smoke
        COMMAND ${CMAKE_COMMAND}
            -DMCP_SERVER=$<TARGET_FILE:cocoa_mcp_server>
            -DPROJECT_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/CocoaProject.json
            -DMCP_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/mcp_initialize.jsonl
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/McpSmoke.cmake
    )
endif()

install(
    TARGETS cocoa_game_sdk sample_game_module
    EXPORT CocoaGameSDKTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/cocoa_game_sdk DESTINATION include)
install(
    EXPORT CocoaGameSDKTargets
    FILE CocoaGameSDKTargets.cmake
    NAMESPACE Cocoa::
    DESTINATION lib/cmake/CocoaGameSDK
)
