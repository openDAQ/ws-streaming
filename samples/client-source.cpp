// This example demonstrates how to implement a TCP client which acts as a data source. It
// connects to a streaming server, then exposes a single time-domain channel, consisting of an
// explicit-rule value signal named "Value" and a linear-rule domain signal named "Time". Press
// Ctrl+C to gracefully shut down the client.

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace std::placeholders;

void on_disconnected(
    std::shared_ptr<boost::asio::signal_set> signal_handler,
    const boost::signals2::connection& slot_connection,
    const boost::system::error_code& ec)
{
    slot_connection.disconnect();

    std::cout << "connection closed (error code: " << ec << ')' << std::endl;
}

void on_connected(
    wss::local_signal& time_signal,
    wss::local_signal& value_signal,
    const boost::system::error_code& ec,
    const std::shared_ptr<wss::connection>& connection)
{
    if (ec)
    {
        std::cerr << "connection failed: " << ec << std::endl;
        return;
    }

    // Register our signals with the connection.
    connection->add_signal(time_signal);
    connection->add_signal(value_signal);

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
}

int main(int argc, char *argv[])
{
    // Declare our linear-rule domain (time) signal.
    wss::local_signal time_signal{
        "/Time",
        wss::metadata_builder{"Time"}
            .data_type(wss::data_types::int64)
            .unit(wss::unit::seconds)
            .linear_rule(0,
                duration_cast<steady_clock::duration>(1ms).count())
            .tick_resolution(
                steady_clock::period::num,
                steady_clock::period::den)
            .build()};

    // Declare our explicit-rule value signals.
    wss::local_signal value_signal{
        "/Value",
        wss::metadata_builder{"Value"}
            .data_type(wss::data_types::real64)
            .unit(wss::unit::volts)
            .range(-10, 10)
            .table(time_signal.id())
            .build()};

    // Allow the hostname/IP to be specified as the first argument, defaulting to "localhost."
    std::string hostname = "localhost";
    if (argc >= 2)
        hostname = argv[1];

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Set up an asynchronous acquisition loop thread which publishes 100 samples every
    // 100 milliseconds, for a total sample rate of 1 kHz or a sample interval of 1ms.
    std::atomic<bool> exit = false;
    std::thread thread{[&]()
    {
        std::vector<double> samples(100);
        auto when = steady_clock::now();

        while (!exit)
        {
            when += 100ms;
            std::this_thread::sleep_until(when);

            value_signal.publish_data(
                when.time_since_epoch().count(),
                samples.size(),
                samples.data(),
                sizeof(decltype(samples)::value_type) * samples.size());
        }
    }};

    // Try to connect to the server; on_connected() will be called on success/failure.
    std::make_shared<wss::client>(ioc.get_executor())
        ->async_connect(
            "ws://" + hostname + ":7414",
            std::bind(on_connected,
                std::ref(time_signal),
                std::ref(value_signal),
                _1,
                _2));

    // Enter the Boost.Asio event loop.
    ioc.run();

    // When the Boost.Asio event loop exits, clean up our asynchronous acquisition loop thread.
    exit = true;
    thread.join();
}
