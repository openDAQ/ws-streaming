# Introduction

This library implements the WebSocket Streaming Protocol in modern C++. The library is built
around [Boost.Asio][boost-asio]. It supports both client and server roles, and in both roles, data
can be published or received using a symmetric API. It is designed for high performance and
reliability. It is platform-independent and can be used on any system supported by Boost.Asio.

# Building the Library

## Dependencies

The library uses a minimal set of dependencies, and will automatically fetch them if they are not
already installed on the host system. As long as CMake and a suitable C++ compiler are installed,
it is usually sufficient to simply clone and build the library without first installing any
additional dependencies.

- Build dependencies:
  - Tools:
    - A C++ compiler supporting [C++17][cpp17] (e.g., [gcc][gcc], [clang][clang], [MSVC][msvc])
    - [CMake][cmake] &ge; 3.24
  - Libraries (automatially fetched if necessary):
    - [GoogleTest] if building unit tests &ge; v1.17.0
    - [Boost][boost] header-only libraries (asio, beast, serialization, signals2) &ge; 1.84
    - [nlohmann-json][nlohmann-json] &ge; v3.12.0

- Runtime dependencies:
  - [Boost][boost] compiled libraries (system, url) &ge; 1.84

## Compiling and Installing

    cmake -B build
    cmake --build build
    cmake --install build

# Using the Library

## CMake Integration

The library uses CMake for building and installation. Other CMake projects can easily use
installed versions of the library via [find_package()][cmake-find-package]. Building the library
in-project is also supported, using [FetchContent][cmake-fetch-content] or
[add_subdirectory()][cmake-add-subdirectory].

### Using an Installed Version of the Library

    find_package(ws-streaming 3.0.0 REQUIRED)

### Fetching the Library and Building In-Project

    FetchContent_Declare(ws-streaming
        GIT_REPOSITORY https://github.com/openDAQ/ws-streaming
        GIT_TAG v3.0.0
        OVERRIDE_FIND_PACKAGE)

    FetchContent_MakeAvailable(ws-streaming)

### Linking to the Library

    target_link_libraries(my-project PUBLIC ws-streaming::ws-streaming)

## Examples

### Basic Examples

These examples demonstrate the basic usage of the library for the most common use-cases.

- [server-source] - Implements a server that publishes synchronous scalar data to connected
                    clients.
- [client-sink] - Implements a client that receives synchronous scalar data from a server.

### Bidirectional Streaming Support

The basic examples above move data in the traditional direction: data is published from a server
to connected clients. The library supports bidirectional streaming, and this direction can be
reversed. These examples demonstrate moving data from a client to a server.

- [client-source] - Implements a client that publishes synchronous scalar data to a server.
- [server-sink] - Implements a server that receives synchronous scalar data from connected
                  clients.

### Other examples

These examples demonstrate other use-cases, like streaming "asynchronous" signals (those with an
explicit-rule domain signal) and structure-valued signals.

- [can-source] - Implements a server that publishes asynchronous raw CAN message structures to
                 connected clients.
- [can-sink] - Implements a client that receives asynchronous raw CAN message structures from a
               server.

[can-sink]: samples/can-sink.cpp
[can-source]: samples/can-source.cpp
[client-sink]: samples/client-sink.cpp
[client-source]: samples/client-source.cpp
[server-sink]: samples/server-sink.cpp
[server-source]: samples/server-source.cpp

[boost]: https://www.boost.org/
[boost-asio]: https://www.boost.org/doc/libs/1_84_0/doc/html/boost_asio.html
[clang]: https://clang.llvm.org/
[cmake]: https://cmake.org/
[cmake-add-subdirectory]: https://cmake.org/cmake/help/latest/command/add_subdirectory.html
[cmake-fetch-content]: https://cmake.org/cmake/help/latest/module/FetchContent.html
[cmake-find-package]: https://cmake.org/cmake/help/latest/command/find_package.html
[cpp17]: https://en.wikipedia.org/wiki/C%2B%2B17
[gcc]: https://gcc.gnu.org/
[googletest]: https://github.com/google/googletest
[msvc]: https://visualstudio.microsoft.com/vs/features/cplusplus/
[nlohmann-json]: https://github.com/nlohmann/json
