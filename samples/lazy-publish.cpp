// This example demonstrates how to use a local_signal's on_subscribed and on_unsubscribed events
// to trigger an acquisition loop only when one or more remote peers are subscribed to a signal.

#include <atomic>
#include <chrono>
#include <iostream>
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
    // This thread will be lazy-activated only when one or more clients are subscribed.

    std::atomic<bool> exit;
    std::thread thread;

    value_signal.on_subscribed.connect([&]()
    {
        std::cout << "value signal subscribed, starting acquisition loop" << std::endl;

        exit = false;
        thread = std::thread{[&]()
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
    });

    value_signal.on_unsubscribed.connect([&]()
    {
        std::cout << "value signal unsubscribed, stopping acquisition loop" << std::endl;

        exit = true;
        thread.join();
    });

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server object. It will use the Boost.Asio execution
    // context to accept incoming connections, publishing the signals we give it.
    wss::server server{ioc.get_executor()};
    server.add_default_listeners();
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
}
