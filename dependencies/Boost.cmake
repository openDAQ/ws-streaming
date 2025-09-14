option(WS_STREAMING_IGNORE_INSTALLED_BOOST "Don't use the preinstalled Boost library, if present" OFF)

if(NOT WS_STREAMING_IGNORE_INSTALLED_BOOST)
    find_package(Boost 1.82 QUIET GLOBAL)
endif()

if(Boost_FOUND)

    message(STATUS "Found Boost ${Boost_VERSION} at ${Boost_CONFIG}")

else()

    message(STATUS "Fetching Boost...")

    FetchContent_Declare(Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
        OVERRIDE_FIND_PACKAGE
    )

    FetchContent_MakeAvailable(Boost)

endif()

function(FixBoostTarget name)
    if(NOT TARGET Boost::${name})
        if(TARGET boost_${name})
            add_library(Boost::${name} ALIAS boost_${name})
        elseif(TARGET Boost::headers)
            get_property(real_target TARGET Boost::headers PROPERTY ALIASED_TARGET)
            if("${real_target}" STREQUAL "")
                set(real_target Boost::headers)
            endif()
            add_library(Boost::${name} ALIAS ${real_target})
        endif()
    endif()
endfunction()

FixBoostTarget(archive)
FixBoostTarget(asio)
FixBoostTarget(beast)
FixBoostTarget(endian)
FixBoostTarget(multiprecision)
FixBoostTarget(serialization)
FixBoostTarget(signals2)
FixBoostTarget(uuid)
