#include <cstddef>
#include <string>

#include <boost/endian/conversion.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/data_types.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

wss::detail::remote_signal_impl::remote_signal_impl(
        const std::string& id)
    : remote_signal(id)
{
}

void wss::detail::remote_signal_impl::subscribe()
{
    if (_subscription_count++ != 0)
        return;

    on_subscribe_requested();
}

void wss::detail::remote_signal_impl::unsubscribe()
{
    if (--_subscription_count != 0)
        return;

    on_unsubscribe_requested();
}

void wss::detail::remote_signal_impl::handle_data(
    const void *data,
    std::size_t size)
{
    if (!_is_subscribed)
        return;

    if (_is_linear)
    {
        if (size >= sizeof(streaming_protocol::linear_payload))
        {
            const auto* payload = static_cast<const streaming_protocol::linear_payload *>(data);
            _value_index = boost::endian::little_to_native(payload->sample_index);
            _linear_value = boost::endian::little_to_native(payload->value);
        }
    }

    std::int64_t linear_value = 0;
    
    if (_domain_signal && _domain_signal->_is_linear)
        linear_value = _domain_signal->_linear_value
            + (_value_index - _domain_signal->_value_index)
            * _domain_signal->_linear_start_delta.second;

    std::size_t sample_count = 0;
    if (_sample_size && !_is_linear)
        sample_count = size / _sample_size;

    on_data_received(linear_value, sample_count, data, size);

    if (_sample_size && !_is_linear)
        _value_index += size / _sample_size;
}

void wss::detail::remote_signal_impl::handle_metadata(
    const std::string& method,
    const nlohmann::json& params)
{
    if (method == "subscribe")
        handle_subscribe();

    else if (method == "unsubscribe")
        handle_unsubscribe();

    else if (method == "signal")
        handle_signal(params);
}

void wss::detail::remote_signal_impl::detach()
{
    if (_is_subscribed)
    {
        _is_subscribed = false;
        _signo = 0;

        on_unsubscribed();
    }

    on_unavailable();

    on_subscribed.disconnect_all_slots();
    on_unsubscribed.disconnect_all_slots();
    on_metadata_changed.disconnect_all_slots();
    on_data_received.disconnect_all_slots();
    on_unavailable.disconnect_all_slots();

    _domain_signal.reset();
}

void wss::detail::remote_signal_impl::signo(unsigned signo)
{
    _signo = signo;
}

void wss::detail::remote_signal_impl::handle_subscribe()
{
    if (_is_subscribed)
        return;

    _is_subscribed = true;
    on_subscribed();
}

void wss::detail::remote_signal_impl::handle_unsubscribe()
{
    if (!_is_subscribed)
        return;

    _is_subscribed = false;
    on_unsubscribed();
}

void wss::detail::remote_signal_impl::handle_signal(
    const nlohmann::json& params)
{
    _metadata = params;

    std::string table_id = metadata().table_id();
    if (table_id.empty())
        _domain_signal.reset();
    else
        _domain_signal = on_signal_sought(table_id).value_or(nullptr);

    _is_linear = metadata().rule() == rule_types::linear_rule;
    _linear_start_delta = metadata().linear_start_delta();
    _value_index = metadata().value_index().value_or(_value_index);
    _sample_size = metadata().sample_size();

    on_metadata_changed();
}

std::int64_t wss::detail::remote_signal_impl::value_index() const noexcept
{
    return _value_index;
}

std::int64_t wss::detail::remote_signal_impl::linear_value() const noexcept
{
    return _linear_value;
}
