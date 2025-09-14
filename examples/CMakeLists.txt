set(sources
    ./can-sink.cpp
    ./can-source.cpp
    ./client-sink.cpp
    ./client-source.cpp
    ./lazy-publish.cpp
    ./server-sink.cpp
    ./server-source.cpp
)

foreach(source ${sources})

    get_filename_component(name ${source} NAME_WE)

    add_executable(${name} ${source})

    set_target_properties(${name} PROPERTIES
        CXX_STANDARD            17
        CXX_STANDARD_REQUIRED   ON
        CXX_EXTENSIONS          OFF
    )

    target_link_libraries(${name}
        PUBLIC
            Threads::Threads
            ws-streaming::ws-streaming
    )

endforeach()
