set(sources
    ./test_base64.cpp
    ./test_dynamic_buffer.cpp
    ./test_metadata.cpp
    ./test_peer_limits.cpp
    ./test_semver.cpp
    ./test_streaming_protocol.cpp
)

if(WS_STREAMING_ENABLE_TLS)

    list(APPEND sources ./test_tls.cpp)

endif()

foreach(source ${sources})

    get_filename_component(name ${source} NAME_WE)

    add_executable(${name} ${source})

    set_target_properties(${name} PROPERTIES
        CXX_STANDARD            17
        CXX_STANDARD_REQUIRED   ON
        CXX_EXTENSIONS          ON)

    target_link_libraries(${name} PUBLIC
        GTest::gtest_main
        ws-streaming::ws-streaming)

    add_test(NAME ${name} COMMAND ${name})

endforeach()

if(WS_STREAMING_ENABLE_TLS)

    add_custom_command(
        TARGET test_tls POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/secrets
                $<TARGET_FILE_DIR:test_tls>/secrets
    )

endif()
