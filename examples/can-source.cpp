// This sample demonstrates how to publish structure-valued data with explicit domain values: in
// this case, raw CAN messages. It shows how to generate the appropriate value and domain signal
// metadata and how to publish (simulated) asynchronously arriving messages. This sample operates
// as a server, but the signal-related code for a client is identical; refer to the
// "client-source" sample.

#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include <ws-streaming/ws-streaming.hpp>

#include "can_message.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    // Declare our explicit-rule domain (time) signal.
    wss::local_signal time_signal{
        "/Time",
        wss::metadata_builder{"Time"}
            .data_type(wss::data_types::uint64_t)
            .unit(wss::unit::nanoseconds)
            .origin(wss::metadata::unix_epoch)
            .table("/Time")
            .build()};

    // Declare our explicit-rule, struct-valued CAN signal.
    wss::local_signal can_signal{
        "/CAN",
        wss::metadata_builder{"CAN"}
            .data_type(wss::data_types::struct_t)
            .struct_field(
                wss::struct_field_builder("ArbId")
                    .data_type(wss::data_types::int32_t))
            .struct_field(
                wss::struct_field_builder("Length")
                    .data_type(wss::data_types::int8_t))
            .struct_field(
                wss::struct_field_builder("Data")
                    .data_type(wss::data_types::uint8_t)
                    .array(64))
            .unit(wss::unit::volts)
            .range(-10, 10)
            .table(time_signal.id())
            .build()};

    // Set up a single-threaded Boost.Asio execution context.
    boost::asio::io_context ioc{1};

    // Create the WebSocket Streaming server and register our signals.
    wss::server server{ioc.get_executor()};
    server.add_default_listeners();
    server.add_local_signal(time_signal);
    server.add_local_signal(can_signal);
    server.run();

    // Set up a signal handler to stop the server when Ctrl+C is pressed.
    boost::asio::signal_set signals{ioc, SIGINT};
    signals.async_wait([&](const boost::system::error_code& ec, int signal)
    {
        if (!ec)
            server.close();
    });

    // Set up an asynchronous acquisition loop thread which publishes 100 samples every
    // 100 milliseconds, for a total sample rate of 1 kHz or a sample interval of 1ms.
    std::atomic<bool> exit = false;
    std::thread thread{[&]()
    {
        std::random_device random_device;
        std::default_random_engine random_engine(random_device());
        auto when = system_clock::now();
        std::uniform_int_distribution<unsigned> uniform(50, 500);
        can_message message { };

        message.message_id = 0x1234;
        message.payload_length = 10;
        std::memcpy(message.payload.data(), "helloworld", message.payload_length);

        while (!exit)
        {
            unsigned mss = uniform(random_engine);
            when += std::chrono::milliseconds(mss);
            std::this_thread::sleep_until(when);

            auto time = duration_cast<duration<std::uint64_t, std::nano>>(when.time_since_epoch());
            time_signal.publish_data(&time, sizeof(time));
            can_signal.publish_data(&message, sizeof(message));
        }
    }};

    // Enter the Boost.Asio event loop.
    ioc.run();

    // When the Boost.Asio event loop exits, clean up our asynchronous acquisition loop thread.
    exit = true;
    thread.join();
}
