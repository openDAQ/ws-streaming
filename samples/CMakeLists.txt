add_executable(wss-test wss-test.cpp)

set_target_properties(wss-test PROPERTIES
    CXX_STANDARD            17
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)

target_link_libraries(wss-test
    PUBLIC
        ws-streaming::ws-streaming
)

add_executable(cli-test cli-test.cpp)

set_target_properties(cli-test PROPERTIES
    CXX_STANDARD            17
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
)

target_link_libraries(cli-test
    PUBLIC
        Boost::headers
        Boost::url
        ws-streaming::ws-streaming
)
