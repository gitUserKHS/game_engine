cmake_minimum_required(VERSION 3.20)

# 이 CMake 프로젝트는 배포 SDK에서 GameModule만 빌드한다.
# 엔진 본체를 다시 컴파일하지 않으므로 AI가 엔진 코어를 우연히 수정할 수 없다.
project(CocoaGameProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(
    CocoaGameSDK 0.5 CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake/CocoaGameSDK"
    NO_DEFAULT_PATH
)

function(cocoa_add_game_module target source)
    add_library(${target} SHARED "${source}")
    target_link_libraries(${target} PRIVATE Cocoa::cocoa_game_sdk)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Game/Binaries"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Game/Binaries"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

cocoa_add_game_module(sample_game_module Game/Source/SampleGameModule.cpp)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Source/AIGameModule.cpp")
    cocoa_add_game_module(ai_game_module Game/Source/AIGameModule.cpp)
endif()
