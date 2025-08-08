#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/metadata.hpp>
#include <ws-streaming/rule_types.hpp>

wss::local_signal::local_signal(
        const std::string& id,
        const wss::metadata& metadata)
    : _id{id}
{
    set_metadata(metadata);
}

void wss::local_signal::set_metadata(const wss::metadata& metadata)
{
    _metadata = metadata;

    _is_linear = _metadata.rule() == rule_types::linear_rule;
    _linear_start_delta = _metadata.linear_start_delta();
    _table_id = _metadata.table_id();

    on_metadata_changed();
}

void wss::local_signal::publish_data(
    const void *data,
    std::size_t size)
    noexcept
{
    on_data_published(0, 0, data, size);
}

void wss::local_signal::publish_data(
    std::int64_t domain_value,
    std::size_t sample_count,
    const void *data,
    std::size_t size)
    noexcept
{
    on_data_published(domain_value, sample_count, data, size);
    _sample_index += sample_count;
}

const std::string& wss::local_signal::id() const noexcept
{
    return _id;
}

bool wss::local_signal::is_linear() const noexcept
{
    return _is_linear;
}

const std::pair<std::int64_t, std::int64_t>
wss::local_signal::linear_start_delta() const noexcept
{
    return _linear_start_delta;
}

const wss::metadata& wss::local_signal::metadata() const noexcept
{
    return _metadata;
}

std::size_t wss::local_signal::sample_index() const noexcept
{
    return _sample_index;
}

const std::string& wss::local_signal::table_id() const noexcept
{
    return _table_id;
}

bool wss::local_signal::is_subscribed() const noexcept
{
    return _subscribe_count > 0;
}

wss::local_signal::subscribe_holder wss::local_signal::increment_subscribe_count()
{
    return subscribe_holder(*this);
}
