find_package(Boost 1.82 QUIET GLOBAL)

if(Boost_FOUND)

    message(STATUS "Found Boost ${Boost_VERSION} at ${Boost_CONFIG}")

else()

    message(STATUS "Fetching Boost...")

    FetchContent_Declare(Boost
        URL             https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
        OVERRIDE_FIND_PACKAGE
    )

    FetchContent_MakeAvailable(Boost)

endif()
