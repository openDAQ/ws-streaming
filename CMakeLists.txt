cmake_minimum_required(VERSION 3.24)

project(ws-streaming VERSION 3.0.0 LANGUAGES CXX)

option(WS_STREAMING_BUILD_EXAMPLES  "Build example programs"            ${PROJECT_IS_TOP_LEVEL})
option(WS_STREAMING_BUILD_TESTS     "Build unit tests"                  ${PROJECT_IS_TOP_LEVEL})
option(WS_STREAMING_INSTALL         "Generate CMake install targets"    ON)

include(CMakePackageConfigHelpers)
include(FetchContent)
include(GNUInstallDirs)

include(dependencies/Boost.cmake)
include(dependencies/nlohmann_json.cmake)
include(dependencies/Threads.cmake)

add_subdirectory(src)

if(WS_STREAMING_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(WS_STREAMING_BUILD_TESTS)
    enable_testing()
    include(dependencies/googletest.cmake)
    add_subdirectory(tests)
endif()
