// This example demonstrates how to implement a TCP client which acts as a data sink. It
// connects to a streaming server, then subscribes to the signal "/CAN", and prints messages to
// the console when data packets are received. The "server-source" sample provides a suitable
// server for this demo to connect to. Press Ctrl+C to gracefully shut down the client.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::placeholders;

struct signal_state
{
    wss::connection_ptr connection;
    wss::remote_signal_ptr value_signal;
    wss::remote_signal_ptr domain_signal;
    boost::signals2::scoped_connection on_metadata_changed;
    boost::signals2::scoped_connection on_domain_data;
    boost::signals2::scoped_connection on_value_data;
    boost::signals2::scoped_connection on_unavailable;
};

void on_disconnected(
    std::shared_ptr<boost::asio::signal_set> signal_handler,
    const boost::signals2::connection& slot_connection,
    const boost::system::error_code& ec)
{
    slot_connection.disconnect();

    std::cout << "connection closed (error code: " << ec << ')' << std::endl;
}

void on_unavailable(std::shared_ptr<signal_state> state)
{
    state->on_metadata_changed.disconnect();
    state->on_domain_data.disconnect();
    state->on_value_data.disconnect();
    state->on_unavailable.disconnect();
}

void on_domain_data_received(
    std::int64_t domain_value,
    std::size_t sample_count,
    const void *data,
    std::size_t size)
{
    std::cout << "received " << size << " domain data byte(s)" << std::endl;
}

void on_metadata_changed(std::shared_ptr<signal_state> state)
{
    std::string domain_signal_id = state->value_signal->metadata().table_id();
    std::cout << state->value_signal->metadata().json().dump() << std::endl;
    std::cout << "domain signal id is " << domain_signal_id << std::endl;

    state->domain_signal = state->connection->find_remote_signal(domain_signal_id);

    if (state->domain_signal)
    {
        std::cout << "got domain signal" << std::endl;
        state->on_domain_data = state->domain_signal->on_data_received.connect(on_domain_data_received);
    }

    else
    {
        state->on_domain_data.disconnect();
    }
}

void on_data_received(
    std::int64_t domain_value,
    std::size_t sample_count,
    const void *data,
    std::size_t size)
{
    std::cout << "received " << size << " data byte(s) with domain value " << domain_value << std::endl;
}

void on_available(
    const wss::connection_ptr& connection,
    const wss::remote_signal_ptr& signal)
{
    std::cout << "available signal: " << signal->id() << std::endl;

    // Subscribe to the signal with ID "/CAN".
    if (signal->id() == "/CAN")
    {
        auto state = std::make_shared<signal_state>();

        state->connection = connection;
        state->value_signal = signal;

        state->on_metadata_changed = signal->on_metadata_changed.connect(
            std::bind(on_metadata_changed, state));
        state->on_value_data = signal->on_data_received.connect(on_data_received);
        state->on_unavailable = signal->on_unavailable.connect(
            std::bind(on_unavailable, state));

        signal->subscribe();
    }
}

void on_connected(
    const boost::system::error_code& ec,
    const wss::connection_ptr& connection)
{
    if (ec)
    {
        std::cerr << "connection failed: " << ec << std::endl;
        return;
    }

    // Set up a signal handler to stop the connection attempt when Ctrl+C is pressed.
    auto signal_handler = std::make_shared<boost::asio::signal_set>(connection->executor(), SIGINT);
    signal_handler->async_wait([connection](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            connection->close();
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
    connection->on_available.connect(
        std::bind(
            on_available,
            connection,
            _1));
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
    wss::client client{ioc.get_executor()};
    client.async_connect(
        "ws://" + hostname + ":7414",
        on_connected);

    ioc.run();
}
