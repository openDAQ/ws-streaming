#include <cstdint>

#include <ws-streaming/metadata.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/detail/linear_table.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

#include <boost/endian/conversion.hpp>

wss::detail::linear_table::linear_table(const metadata& metadata)
{
    update(metadata);
}

void wss::detail::linear_table::update(
    const metadata& metadata)
{
    auto [start, delta] = metadata.linear_start_delta();

    _value = start.value_or(_value);
    _delta = delta.value_or(_delta);

    if (auto new_index = metadata.value_index(); new_index.has_value())
    {
        _index = new_index.value();
        _driven_index = _index;
    }
}

void wss::detail::linear_table::update(
    const streaming_protocol::linear_payload& payload)
{
    _driven_index = _index = boost::endian::little_to_native(payload.sample_index);
    _value = boost::endian::little_to_native(payload.value);
}

std::int64_t wss::detail::linear_table::driven_value() const noexcept
{
    return _value + _delta * (_driven_index - _index);
}

std::int64_t wss::detail::linear_table::value_at(std::int64_t index) const noexcept
{
    return _value + _delta * (index - _index);
}

void wss::detail::linear_table::set(std::int64_t index, std::int64_t value) noexcept
{
    _index = index;
    _value = value;
    _driven_index = index;
}

void wss::detail::linear_table::drive_to(std::int64_t index) noexcept
{
    _driven_index = index;
}

std::int64_t wss::detail::linear_table::driven_index() const noexcept
{
    return _driven_index;
}
