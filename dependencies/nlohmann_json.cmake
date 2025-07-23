find_package(nlohmann_json 3.12.0 QUIET GLOBAL)

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
