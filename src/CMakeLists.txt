file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE headers ../include/*.hpp)

add_library(${PROJECT_NAME} ${sources} ${headers})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD            17
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)

# TODO REMOVEME!!!
target_compile_options(${PROJECT_NAME} PUBLIC -Wall -pedantic)

target_compile_definitions(
    ${PROJECT_NAME}
        PRIVATE
            WS_STREAMING_VERSION_MAJOR="${PROJECT_VERSION_MAJOR}"
            WS_STREAMING_VERSION_MINOR="${PROJECT_VERSION_MINOR}"
            WS_STREAMING_VERSION_PATCH="${PROJECT_VERSION_PATCH}"
            WS_STREAMING_VERSION_TWEAK="${PROJECT_VERSION_TWEAK}"
)

target_include_directories(
    ${PROJECT_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            .
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Boost::headers
        Boost::url
        nlohmann_json::nlohmann_json
)

if(TARGET Boost::asio)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::asio)
endif()

if(TARGET Boost::signals2)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::signals2)
endif()
