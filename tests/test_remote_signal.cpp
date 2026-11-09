// Tests for the data handling of wss::detail::remote_signal_impl.
//
// A constant-rule signal sends entries of a 64-bit value index followed by the value, the index
// telling from which sample of its domain table the value holds. The tests drive a linear time
// signal and a constant-rule signal that uses its table directly, without a connection.

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

using wss::detail::remote_signal_impl;

namespace
{
    struct received
    {
        std::int64_t domain_value;
        std::size_t sample_count;
        std::int64_t value;
    };

    // A time signal with a linear table that sample index 100 maps to 5000 and each sample adds 10
    std::shared_ptr<remote_signal_impl> make_time_signal()
    {
        auto time = std::make_shared<remote_signal_impl>("time");
        time->handle_metadata("subscribe", nlohmann::json::object());
        time->handle_metadata("signal", {
            { "tableId", "table" },
            { "definition", {
                { "name", "time" },
                { "rule", "linear" },
                { "linear", { { "delta", 10 } } },
                { "dataType", "uint64" },
            } },
        });

        const wss::detail::streaming_protocol::linear_payload start{100, 5000};
        time->handle_data(&start, sizeof(start));
        return time;
    }

    std::shared_ptr<remote_signal_impl> make_constant_signal(const std::shared_ptr<remote_signal_impl>& time)
    {
        auto status = std::make_shared<remote_signal_impl>("status");
        status->on_table_sought.connect([time](const std::string&) { return time; });
        status->handle_metadata("subscribe", nlohmann::json::object());
        status->handle_metadata("signal", {
            { "tableId", "table" },
            { "definition", {
                { "name", "status" },
                { "rule", "constant" },
                { "dataType", "int64" },
            } },
        });
        return status;
    }

    void append_entry(std::vector<std::uint8_t>& out, std::uint64_t index, std::int64_t value)
    {
        const auto at = out.size();
        out.resize(at + sizeof(index) + sizeof(value));
        std::memcpy(out.data() + at, &index, sizeof(index));
        std::memcpy(out.data() + at + sizeof(index), &value, sizeof(value));
    }
}

TEST(RemoteSignalConstantRule, DeliversEachEntryAtTheDomainValueOfItsIndex)
{
    auto time = make_time_signal();
    auto status = make_constant_signal(time);

    std::vector<received> data;
    status->on_data_received.connect(
        [&](std::int64_t domain_value, std::size_t sample_count, const void *values, std::size_t size)
        {
            ASSERT_EQ(size, sizeof(std::int64_t));
            std::int64_t value;
            std::memcpy(&value, values, sizeof(value));
            data.push_back({domain_value, sample_count, value});
        });

    std::vector<std::uint8_t> packet;
    append_entry(packet, 104, 7);
    append_entry(packet, 110, -3);
    status->handle_data(packet.data(), packet.size());

    ASSERT_EQ(data.size(), 2u);
    EXPECT_EQ(data[0].domain_value, 5040);
    EXPECT_EQ(data[0].sample_count, 1u);
    EXPECT_EQ(data[0].value, 7);
    EXPECT_EQ(data[1].domain_value, 5100);
    EXPECT_EQ(data[1].sample_count, 1u);
    EXPECT_EQ(data[1].value, -3);
}

TEST(RemoteSignalConstantRule, IgnoresAnIncompleteEntry)
{
    auto time = make_time_signal();
    auto status = make_constant_signal(time);

    std::size_t calls = 0;
    status->on_data_received.connect([&](std::int64_t, std::size_t, const void *, std::size_t) { ++calls; });

    std::vector<std::uint8_t> packet;
    append_entry(packet, 104, 7);
    packet.resize(packet.size() + 10);
    status->handle_data(packet.data(), packet.size());

    EXPECT_EQ(calls, 1u);
}
