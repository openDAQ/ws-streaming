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
        nlohmann_json::nlohmann_json
)
