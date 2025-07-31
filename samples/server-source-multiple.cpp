// This example demonstrates how to implement a TCP server which acts as a data source. It exposes
// two time-domain channels, consisting of explicit-rule value signals named "Value1" and "Value2"
// and a common linear-rule domain signal named "Time". Press Ctrl+C to gracefully shut down the
// server.

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;

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
    std::array<wss::local_signal, 2> value_signals
    {
        wss::local_signal{
            "/Value1",
            wss::metadata_builder{"Value1"}
                .data_type(wss::data_types::real64)
                .unit(wss::unit::volts)
                .range(-10, 10)
                .table(time_signal.id())
                .build()},
        wss::local_signal{
            "/Value2",
            wss::metadata_builder{"Value2"}
                .data_type(wss::data_types::real64)
                .unit(wss::unit::volts)
                .range(-10, 10)
                .table(time_signal.id())
                .build()},
    };

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server and register our signals.
    wss::server server{ioc.get_executor()};
    server.add_signal(time_signal);
    for (auto& value_signal : value_signals)
        server.add_signal(value_signal);
    server.run();

    // Set up a signal handler to stop the server when Ctrl+C is pressed.
    boost::asio::signal_set signals{ioc, SIGINT};
    signals.async_wait([&](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            server.stop();
    });

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

            for (auto& value_signal : value_signals)
                value_signal.publish_data(
                    when.time_since_epoch().count(),
                    samples.size(),
                    samples.data(),
                    sizeof(decltype(samples)::value_type) * samples.size());
        }
    }};

    // Enter the Boost.Asio event loop.
    ioc.run();

    // When the Boost.Asio event loop exits, clean up our asynchronous acquisition loop thread.
    exit = true;
    thread.join();

    return 0;
}
