// This example demonstrates how to implement a TCP client which acts as a data source. It
// connects to a streaming server, then exposes a single time-domain channel, consisting of an
// explicit-rule value signal named "Value" and a linear-rule domain signal named "Time". Press
// Ctrl+C to gracefully shut down the client.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace std::placeholders;
using namespace std::string_literals;

// Configurable constants.
static constexpr unsigned sample_rate = 1000;
static constexpr unsigned block_rate = 10;

// Forward declarations, so we can order the code as it executes chronologically.
void on_disconnected(std::shared_ptr<boost::asio::signal_set> signal_handler, const boost::signals2::connection& slot_connection, const boost::system::error_code& ec);
void on_connected(wss::local_signal& time_signal, wss::local_signal& value_signal, const boost::system::error_code& ec, const wss::connection_ptr& connection);

int main(int argc, char *argv[])
{
    // Declare a linear-rule domain (time) signal.
    wss::local_signal time_signal{
        "/Time",
        wss::metadata_builder{"Time"}
            .data_type(wss::data_types::int64_t)
            .unit(wss::unit::seconds)
            .linear_rule(0,
                duration_cast<system_clock::duration>(1s).count() / sample_rate)
            .tick_resolution(
                system_clock::period::num,
                system_clock::period::den)
            .table("/Time")
            .build()};

    // Declare an explicit-rule value signal, referencing
    // the time signal above as its domain ("table").
    wss::local_signal value_signal{
        "/Value",
        wss::metadata_builder{"Value"}
            .data_type(wss::data_types::real64_t)
            .unit(wss::unit::volts)
            .range(-10, 10)
            .table(time_signal.id())
            .build()};

    // Set up an asynchronous acquisition loop thread which publishes 100 samples every
    // 100 milliseconds, for a total sample rate of 1 kHz or a sample interval of 1ms.
    std::atomic<bool> exit{false};
    std::thread thread{[&]()
    {
        std::vector<double> samples(sample_rate / block_rate);
        auto when = system_clock::now();
        std::uint64_t t = 0;

        while (!exit)
        {
            when += duration_cast<system_clock::duration>(1s) / block_rate;
            std::this_thread::sleep_until(when);

            // Make a sine wave with a period of 2*pi seconds.
            for (std::size_t i = 0; i < samples.size(); ++i)
                samples[i] = std::sin(++t / static_cast<double>(sample_rate));

            // It is safe to call wss::local_signal::publish_data() from any thread without
            // explicit synchronization. However, *we* must not access or call any members of
            // local_signal concurrently.
            value_signal.publish_data(
                when.time_since_epoch().count(),
                samples.size(),
                samples.data(),
                sizeof(decltype(samples)::value_type) * samples.size());
        }
    }};

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Try to connect to the server; on_connected() will be called on success/failure.
    wss::client client{ioc.get_executor()};
    client.async_connect(
        boost::url_view("ws://"s + (argc >= 2 ? argv[1] : "localhost") + ":7414"),
        std::bind(on_connected,
            std::ref(time_signal),
            std::ref(value_signal),
            _1,
            _2));

    // Enter the Boost.Asio event loop. This function will return when there are no more
    // scheduled asynchronous work items - this happens when the connection has been closed.
    ioc.run();

    // When the Boost.Asio event loop exits, clean up our asynchronous acquisition loop thread.
    exit = true;
    thread.join();
}

void on_connected(
    wss::local_signal& time_signal,
    wss::local_signal& value_signal,
    const boost::system::error_code& ec,
    const wss::connection_ptr& connection)
{
    if (ec)
    {
        std::cerr << "connection failed: " << ec << std::endl;
        return;
    }

    // Register our signals with the connection.
    connection->add_local_signal(time_signal);
    connection->add_local_signal(value_signal);

    // Set up a Boost.Asio signal handler to gracefully close the connection when Ctrl+C is
    // pressed. We will transfer ownership of the signal handler to the on_disconnected event
    // handler below, so we can stop it if the connection is closed on its own. Failure to do this
    // would block the Boost.Asio io_context from exiting when the connection is closed.
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
}

void on_disconnected(
    std::shared_ptr<boost::asio::signal_set> signal_handler,
    const boost::signals2::connection& slot_connection,
    const boost::system::error_code& ec)
{
    slot_connection.disconnect();

    std::cout << "connection closed (error code: " << ec << ')' << std::endl;
}
