// This example demonstrates how to implement a TCP client which acts as a data sink. It
// subscribes to the signal "/Value", and prints messages to the console when data packets are
// received. The "server-source" sample provides a suitable server for this demo to connect to.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::placeholders;

void on_disconnected(
    std::shared_ptr<wss::connection> connection,
    std::shared_ptr<boost::asio::signal_set> signal_handler,
    const boost::signals2::connection& slot_connection)
{
    slot_connection.disconnect();
}

void on_data_received(std::int64_t domain_value, const void *data, std::size_t size)
{
    std::cout << "data received: domain=" << domain_value << std::endl;
}

void on_available(const std::shared_ptr<wss::remote_signal>& signal)
{
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
    std::cout << "setting up signal handler" << std::endl;
    signal_handler->async_wait([connection](const boost::system::error_code& ec, int signal)
    {
        std::cout << "signal_handler " << ec << std::endl;
        if (!ec)
            connection->stop();
    });

    connection->on_disconnected.connect_extended(
        decltype(connection->on_disconnected)::extended_slot_type{
            &on_disconnected,
            connection,
            signal_handler,
            _1});

    connection->on_available.connect(on_available);
}

int main(int argc, char *argv[])
{
    std::string hostname = "localhost";
    if (argc >= 2)
        hostname = argv[1];

    boost::asio::io_context ioc{1};

    std::make_shared<wss::client>(ioc.get_executor())
        ->async_connect(
            "ws://" + hostname + ":7414",
            on_connected);

    ioc.run();
}
