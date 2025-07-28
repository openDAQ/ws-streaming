file(GLOB sources *.cpp)

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
