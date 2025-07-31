option(WS_STREAMING_IGNORE_INSTALLED_NLOHMANN_JSON "Don't use the preinstalled nlohmann/json library, if present" OFF)

if(NOT WS_STREAMING_IGNORE_INSTALLED_NLOHMANN_JSON)
    find_package(nlohmann_json 3.12.0 QUIET GLOBAL)
endif()

if(nlohmann_json_FOUND)

    message(STATUS "Found nlohmann_json ${nlohmann_json_VERSION} at ${nlohmann_json_CONFIG}")

else()

    message(STATUS "Fetching nlohmann_json...")

    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY  https://github.com/nlohmann/json
        GIT_TAG         v3.12.0
        GIT_SHALLOW
        OVERRIDE_FIND_PACKAGE
    )

    FetchContent_MakeAvailable(nlohmann_json)

endif()
