message(STATUS "Fetching googletest...")

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(googletest)
