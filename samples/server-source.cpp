// This example demonstrates how to implement a TCP server which acts as a data source. It exposes
// a single time-domain channel, consisting of an explicit-rule value signal named "Value" and a
// linear-rule domain signal named "Time". Press Ctrl+C to gracefully shut down the server.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;

// Configurable constants.
static constexpr unsigned sample_rate = 1000;
static constexpr unsigned block_rate = 10;

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
            .origin(wss::metadata::unix_epoch)
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
    std::atomic<bool> exit = false;
    std::thread thread{[&]()
    {
        std::vector<double> samples(sample_rate / block_rate);
        auto when = system_clock::now();

        while (!exit)
        {
            when += duration_cast<system_clock::duration>(1s) / block_rate;
            std::this_thread::sleep_until(when);

            value_signal.publish_data(
                when.time_since_epoch().count(),
                samples.size(),
                samples.data(),
                sizeof(decltype(samples)::value_type) * samples.size());
        }
    }};

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server object. It will use the Boost.Asio execution
    // context to accept incoming connections, publishing the signals we give it.
    wss::server server{ioc.get_executor()};
    server.add_local_signal(time_signal);
    server.add_local_signal(value_signal);
    server.run();

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

    // When the Boost.Asio event loop exits, clean up our asynchronous acquisition loop thread.
    exit = true;
    thread.join();
}
