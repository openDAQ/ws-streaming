// Tests a constant-rule signal from a server to a client over a loopback connection.
//
// A constant-rule signal on a linear table goes out as entries of a value index and a value. The
// server finds the index of each published domain value on the table; the client maps it back
// through its own copy of the table, which follows the server's in stream order. The tests check
// that each value arrives with the domain value it was published at, also before the table's
// explicit signal has set the table, and whichever of the two signals goes first.

#include <chrono>
#include <cstdint>
#include <future>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <ws-streaming/ws-streaming.hpp>

using namespace std::chrono_literals;

namespace
{
    constexpr std::uint16_t PORT = 17431;

    struct received
    {
        std::int64_t domain_value;
        std::int64_t value;
    };

    class ConstantRuleTest : public testing::Test
    {
        protected:

            boost::asio::io_context ioc{1};

            wss::local_signal time_signal{"/Time",
                wss::metadata_builder{"Time"}
                    .data_type(wss::data_types::int64_t)
                    .linear_rule(0, 10)
                    .table("/Time")
                    .build()};

            wss::local_signal value_signal{"/Value",
                wss::metadata_builder{"Value"}
                    .data_type(wss::data_types::int64_t)
                    .table("/Time")
                    .build()};

            wss::local_signal status_signal{"/Status",
                wss::metadata_builder{"Status"}
                    .data_type(wss::data_types::int64_t)
                    .constant_rule()
                    .table("/Time")
                    .build()};

            // Connects a client, subscribes the value and status signals, then runs publish on the I/O thread and
            // returns what the status and value signals received once the status signal has expected entries
            std::pair<std::vector<received>, std::vector<std::int64_t>> exchange(
                const std::function<void()>& publish,
                std::size_t expected)
            {
                wss::server server{ioc.get_executor()};
                server.add_listener(PORT);
                server.add_local_signal(time_signal);
                server.add_local_signal(value_signal);
                server.add_local_signal(status_signal);
                server.run();

                std::vector<received> status_data;
                std::vector<std::int64_t> value_domains;
                std::promise<void> done;
                unsigned subscribed = 0;
                std::vector<boost::signals2::scoped_connection> connections;
                wss::connection_ptr held;

                wss::client client{ioc.get_executor()};
                client.async_connect("ws://127.0.0.1:" + std::to_string(PORT) + "/",
                    [&](const boost::system::error_code& ec, wss::connection_ptr connection)
                    {
                        ASSERT_FALSE(ec) << ec.message();
                        held = connection;
                        connections.emplace_back(connection->on_available.connect(
                            [&](wss::remote_signal_ptr signal)
                            {
                                if (signal->id() != "/Status" && signal->id() != "/Value")
                                    return;

                                connections.emplace_back(signal->on_subscribed.connect(
                                    [&]
                                    {
                                        if (++subscribed == 2)
                                            boost::asio::post(ioc, publish);
                                    }));

                                connections.emplace_back(signal->on_data_received.connect(
                                    [&, id = signal->id()](std::int64_t domain_value, std::size_t, const void *data, std::size_t size)
                                    {
                                        if (id == "/Value")
                                            return value_domains.push_back(domain_value);

                                        ASSERT_EQ(size, sizeof(std::int64_t));
                                        status_data.push_back({domain_value, *static_cast<const std::int64_t *>(data)});
                                        if (status_data.size() == expected)
                                            done.set_value();
                                    }));

                                signal->subscribe();
                            }));
                    });

                std::thread io_thread{[this] { ioc.run(); }};
                const bool arrived = done.get_future().wait_for(5s) == std::future_status::ready;
                boost::asio::post(ioc, [&] { held.reset(); connections.clear(); server.close(); });
                ioc.stop();
                io_thread.join();

                EXPECT_TRUE(arrived) << "the status signal got " << status_data.size() << " of " << expected << " entries";
                return {status_data, value_domains};
            }
    };
}

// The first status value goes out before any value data has set the table; it still arrives at its domain value
TEST_F(ConstantRuleTest, EachValueArrivesWithTheDomainValueItWasPublishedAt)
{
    const std::vector<std::int64_t> samples(10, 1);
    const std::int64_t start_value = 7;
    const std::int64_t changed_value = -3;

    auto [status, value_domains] = exchange([&]
    {
        status_signal.publish_data(1000, 1, &start_value, sizeof(start_value));
        value_signal.publish_data(1000, samples.size(), samples.data(), samples.size() * sizeof(samples[0]));
        status_signal.publish_data(1130, 1, &changed_value, sizeof(changed_value));
        value_signal.publish_data(1100, samples.size(), samples.data(), samples.size() * sizeof(samples[0]));
    }, 2);

    ASSERT_EQ(status.size(), 2u);
    EXPECT_EQ(status[0].domain_value, 1000);
    EXPECT_EQ(status[0].value, start_value);
    EXPECT_EQ(status[1].domain_value, 1130);
    EXPECT_EQ(status[1].value, changed_value);
}

// A constant value published after value data, or for an earlier domain value, keeps its domain value, and so does the value data
TEST_F(ConstantRuleTest, ValueDataAndConstantValuesKeepTheirDomainValuesInAnyOrder)
{
    const std::vector<std::int64_t> samples(10, 1);
    const std::int64_t value = 5;

    auto [status, value_domains] = exchange([&]
    {
        value_signal.publish_data(1000, samples.size(), samples.data(), samples.size() * sizeof(samples[0]));
        status_signal.publish_data(1250, 1, &value, sizeof(value));
        value_signal.publish_data(1100, samples.size(), samples.data(), samples.size() * sizeof(samples[0]));
        status_signal.publish_data(1150, 1, &value, sizeof(value));
    }, 2);

    ASSERT_EQ(value_domains.size(), 2u);
    EXPECT_EQ(value_domains[0], 1000);
    EXPECT_EQ(value_domains[1], 1100);
    ASSERT_EQ(status.size(), 2u);
    EXPECT_EQ(status[0].domain_value, 1250);
    EXPECT_EQ(status[1].domain_value, 1150);
}
