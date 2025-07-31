// This example demonstrates how to implement a TCP client which acts as a data sink. It
// connects to a streaming server, then subscribes to the signal "/Value", and prints messages to
// the console when data packets are received. The "server-source" sample provides a suitable
// server for this demo to connect to. Press Ctrl+C to gracefully shut down the client.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::placeholders;

void on_disconnected(
    std::shared_ptr<boost::asio::signal_set> signal_handler,
    const boost::signals2::connection& slot_connection,
    const boost::system::error_code& ec)
{
    slot_connection.disconnect();

    std::cout << "connection closed (error code: " << ec << ')' << std::endl;
}

void on_data_received(std::int64_t domain_value, const void *data, std::size_t size)
{
    std::cout << "received " << size << " data byte(s) with domain value " << domain_value << std::endl;
}

void on_available(const std::shared_ptr<wss::remote_signal>& signal)
{
    std::cout << "available signal: " << signal->id() << std::endl;

    // Subscribe to the signal with ID "/Value".
    if (signal->id() == "/Value")
    {
        signal->on_data_received.connect(on_data_received);
        signal->subscribe();
    }
}

void on_connected(
    const boost::system::error_code& ec,
    const std::shared_ptr<wss::connection>& connection)
{
    if (ec)
    {
        std::cerr << "connection failed: " << ec << std::endl;
        return;
    }

    // Set up a signal handler to stop the connection attempt when Ctrl+C is pressed.
    auto signal_handler = std::make_shared<boost::asio::signal_set>(connection->get_executor(), SIGINT);
    signal_handler->async_wait([connection](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            connection->stop();
    });

    // We wish to keep the Ctrl+C signal handler active until the connection is closed. To do
    // this, we store a shared_ptr for it in the Boost.Signals2 slot connection for
    // on_disconnected, which, when called, will disconnect itself and thereby destroy the signal
    // handler.
    connection->on_disconnected.connect_extended(
        decltype(connection->on_disconnected)::extended_slot_type{
            &on_disconnected,
            signal_handler,
            _1,
            _2});

    // Call on_available when any signal becomes available from the server.
    connection->on_available.connect(on_available);
}

int main(int argc, char *argv[])
{
    // Allow the hostname/IP to be specified as the first argument, defaulting to "localhost."
    std::string hostname = "localhost";
    if (argc >= 2)
        hostname = argv[1];

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Try to connect to the server; on_connected() will be called on success/failure.
    std::make_shared<wss::client>(ioc.get_executor())
        ->async_connect(
            "ws://" + hostname + ":7414",
            on_connected);

    ioc.run();
}
