#include <cstddef>
#include <cstdint>
#include <string>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/metadata.hpp>

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
}

const std::string& wss::local_signal::id() const noexcept
{
    return _id;
}

const wss::metadata& wss::local_signal::metadata() const noexcept
{
    return _metadata;
}

bool wss::local_signal::is_subscribed() const noexcept
{
    return _subscribe_count > 0;
}

wss::local_signal::subscribe_holder wss::local_signal::increment_subscribe_count()
{
    return subscribe_holder(*this);
}
