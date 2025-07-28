cmake_minimum_required(VERSION 3.24)

project(ws-streaming VERSION 1.0.0 LANGUAGES CXX)

option(WS_STREAMING_ENABLE_SAMPLES  "Build sample programs" ON)
option(WS_STREAMING_ENABLE_TESTS    "Build unit tests"      ON)

include(FetchContent)
include(GNUInstallDirs)

include(dependencies/Boost.cmake)
include(dependencies/nlohmann_json.cmake)

add_subdirectory(src)

if(WS_STREAMING_ENABLE_SAMPLES)
    add_subdirectory(samples)
endif()

if(WS_STREAMING_ENABLE_TESTS)
    enable_testing()
    include(dependencies/googletest.cmake)
    add_subdirectory(tests)
endif()
