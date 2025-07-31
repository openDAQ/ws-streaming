// This example demonstrates how to implement a TCP server which acts as a data sink. When any
// streaming client connects, it subscribes to that client's signal "/Value", and prints messages
// to the console when data packets are received. The "client-source" sample provides a suitable
// client to connect to this demo. Press Ctrl+C to gracefully shut down the server.

#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/ws-streaming.hpp>

void on_data_received(std::int64_t domain_value, const void *data, std::size_t size)
{
    std::cout << "received " << size << " data byte(s) with domain value " << domain_value << std::endl;
}

void on_available(
    const std::shared_ptr<wss::connection>& connection,
    const std::shared_ptr<wss::remote_signal>& signal)
{
    std::cout << "available signal: " << signal->id() << std::endl;

    // Subscribe to the signal with ID "/Value".
    if (signal->id() == "/Value")
    {
        signal->on_data_received.connect(on_data_received);
        signal->subscribe();
    }
}

int main(int argc, char *argv[])
{
    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server and register our signals.
    wss::server server{ioc.get_executor()};
    server.run();

    // Call on_available when any signal becomes available from any client.
    server.on_available.connect(on_available);

    // Set up a signal handler to stop the server when Ctrl+C is pressed.
    boost::asio::signal_set signals{ioc, SIGINT};
    signals.async_wait([&](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            server.stop();
    });

    // Enter the Boost.Asio event loop.
    ioc.run();
}
