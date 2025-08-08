file(GLOB_RECURSE sources *.cpp)
file(GLOB_RECURSE headers ../include/*.hpp)

add_library(${PROJECT_NAME} ${sources} ${headers})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD            17
    CXX_STANDARD_REQUIRED   ON
    CXX_EXTENSIONS          OFF
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
        Boost::headers
        Boost::url
        nlohmann_json::nlohmann_json
)

if(TARGET Boost::asio)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::asio)
endif()

if(TARGET Boost::beast)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::beast)
endif()

if(TARGET Boost::serialization)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::serialization)
endif()

if(TARGET Boost::signals2)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::signals2)
endif()

if(TARGET Boost::uuid)
    target_link_libraries(${PROJECT_NAME} PUBLIC Boost::uuid)
endif()

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
