file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE headers *.hpp)

add_executable(wss-test ${sources} ${headers})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD            17
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)

target_link_libraries(wss-test
    PUBLIC
        ws-streaming::ws-streaming
)
