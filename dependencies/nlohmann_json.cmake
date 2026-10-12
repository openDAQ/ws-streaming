find_package(nlohmann_json 3.10.5 QUIET GLOBAL)

if(nlohmann_json_FOUND)

    message(STATUS "Found nlohmann_json ${nlohmann_json_VERSION} at ${nlohmann_json_CONFIG}")

elseif(TARGET nlohmann_json::nlohmann_json)

    message(STATUS "Found nlohmann_json::nlohmann_json as existing target")

else()

    message(STATUS "Fetching nlohmann_json...")

    function(FetchNlohmannJson)

        set(JSON_Install ON)

        # keeps warnings in its headers out of the consumers' -Werror; FetchContent takes SYSTEM from CMake 3.25
        set(system)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
            set(system SYSTEM)
        endif()

        FetchContent_Declare(nlohmann_json
            GIT_REPOSITORY  https://github.com/nlohmann/json
            GIT_TAG         v3.10.5
            GIT_SHALLOW
            OVERRIDE_FIND_PACKAGE
            ${system}
        )

        FetchContent_MakeAvailable(nlohmann_json)

    endfunction()

    FetchNlohmannJson()

endif()
