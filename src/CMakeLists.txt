set(sources
    ./client.cpp
    ./connection.cpp
    ./detail/command_interface_client_factory.cpp
    ./detail/http_client.cpp
    ./detail/http_client_servicer.cpp
    ./detail/http_command_interface_client.cpp
    ./detail/in_band_command_interface_client.cpp
    ./detail/linear_table.cpp
    ./detail/local_signal_container.cpp
    ./detail/peer.cpp
    ./detail/remote_signal_container.cpp
    ./detail/remote_signal_impl.cpp
    ./detail/semver.cpp
    ./detail/streaming_protocol.cpp
    ./detail/url.cpp
    ./detail/websocket_protocol.cpp
    ./dimension_builder.cpp
    ./local_signal.cpp
    ./metadata_builder.cpp
    ./metadata.cpp
    ./remote_signal.cpp
    ./server.cpp
    ./struct_field_builder.cpp
    ./unit.cpp
)

set(headers
    ../include/ws-streaming/client.hpp
    ../include/ws-streaming/connection.hpp
    ../include/ws-streaming/data_types.hpp
    ../include/ws-streaming/detail/base64.hpp
    ../include/ws-streaming/detail/command_interface_client_factory.hpp
    ../include/ws-streaming/detail/command_interface_client.hpp
    ../include/ws-streaming/detail/connected_client.hpp
    ../include/ws-streaming/detail/connected_client_iterator.hpp
    ../include/ws-streaming/detail/http_client.hpp
    ../include/ws-streaming/detail/http_client_servicer.hpp
    ../include/ws-streaming/detail/http_command_interface_client.hpp
    ../include/ws-streaming/detail/in_band_command_interface_client.hpp
    ../include/ws-streaming/detail/json.hpp
    ../include/ws-streaming/detail/linear_table.hpp
    ../include/ws-streaming/detail/local_signal_container.hpp
    ../include/ws-streaming/detail/peer.hpp
    ../include/ws-streaming/detail/registered_local_signal.hpp
    ../include/ws-streaming/detail/remote_signal_container.hpp
    ../include/ws-streaming/detail/remote_signal_impl.hpp
    ../include/ws-streaming/detail/semver.hpp
    ../include/ws-streaming/detail/streaming_protocol.hpp
    ../include/ws-streaming/detail/url.hpp
    ../include/ws-streaming/detail/websocket_protocol.hpp
    ../include/ws-streaming/dimension_builder.hpp
    ../include/ws-streaming/endianness.hpp
    ../include/ws-streaming/json_rpc_exception.hpp
    ../include/ws-streaming/listener.hpp
    ../include/ws-streaming/local_signal.hpp
    ../include/ws-streaming/metadata_builder.hpp
    ../include/ws-streaming/metadata.hpp
    ../include/ws-streaming/quantities.hpp
    ../include/ws-streaming/remote_signal.hpp
    ../include/ws-streaming/rule_types.hpp
    ../include/ws-streaming/server.hpp
    ../include/ws-streaming/struct_field_builder.hpp
    ../include/ws-streaming/unit.hpp
    ../include/ws-streaming/ws-streaming.hpp
)

add_library(${PROJECT_NAME} ${sources} ${headers})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD                17
    CXX_STANDARD_REQUIRED       ON
    CXX_EXTENSIONS              OFF
    POSITION_INDEPENDENT_CODE   ON
)

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
        Boost::archive
        Boost::asio
        Boost::beast
        Boost::endian
        Boost::multiprecision
        Boost::serialization
        Boost::signals2
        Boost::uuid
        nlohmann_json::nlohmann_json
)

if(WS_STREAMING_INSTALL)

    if(NOT Boost_FOUND)
        message(FATAL_ERROR
            "WS_STREAMING_INSTALL is ON, but Boost was fetched instead of using an installed"
            " version. This is not supported at this time. Please install Boost first, or turn"
            " off WS_STREAMING_INSTALL.")
    endif()

    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(
        DIRECTORY ../include/ws-streaming
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN *.hpp
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
        VERSION ${Upstream_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        EXPORT ${PROJECT_NAME}
        FILE ${PROJECT_NAME}-targets.cmake
        NAMESPACE ws-streaming::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    configure_package_config_file(
        ../cmake/${PROJECT_NAME}-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

endif()
