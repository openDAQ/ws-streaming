// This example demonstrates how to implement a TCP server which acts as a data sink. When any
// streaming client connects, it subscribes to that client's signal "/Value", and prints messages
// to the console when data packets are received. The "client-source" sample provides a suitable
// client to connect to this demo. Press Ctrl+C to gracefully shut down the server.

#include <cstddef>
#include <cstdint>
#include <iostream>

#include <boost/asio.hpp>

#include <ws-streaming/ws-streaming.hpp>

// Forward declarations, so we can order the code as it executes chronologically.
void on_data_received(std::int64_t domain_value, const void *data, std::size_t size);
void on_available(const wss::connection_ptr& connection, const wss::remote_signal_ptr& signal);

int main(int argc, char *argv[])
{
    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server object. It will use the Boost.Asio execution context
    // to accept incoming connections, generating Boost.Signals2 signals (events) we can react to.
    wss::server server{ioc.get_executor()};
    server.add_default_listeners();
    server.run();

    // Attach a Boost.Signals2 "slot" (event handler) which will be called whenever a
    // signal becomes available from any client that has connected to the server.
    server.on_available.connect(on_available);

    // Set up a Boost.Asio signal handler to gracefully close the server when Ctrl+C is pressed.
    boost::asio::signal_set signals{ioc, SIGINT};
    signals.async_wait([&](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            server.close();
    });

    // Enter the Boost.Asio event loop. This function will return when there are no more scheduled
    // asynchronous work items - this happens when the server has been closed by the Ctrl+C signal
    // handler above.
    ioc.run();
}

void on_available(
    const wss::connection_ptr& connection,
    const wss::remote_signal_ptr& signal)
{
    std::cout << "signal available from "
        << connection->socket().remote_endpoint().address()
        << ':' << connection->socket().remote_endpoint().port()
        << ": " << signal->id() << std::endl;

    // Subscribe to the signal if its ID is "/Value".
    if (signal->id() == "/Value")
    {
        signal->subscribe();

        // If desired, we can std::bind() additional arguments to the data handler, for example
        // to give it access to the wss::remote_signal or wss::connection objects.
        signal->on_data_received.connect(on_data_received);
    }
}

void on_data_received(std::int64_t domain_value, const void *data, std::size_t size)
{
    std::cout << "received " << size << " data byte(s) with domain value " << domain_value << std::endl;
}
