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

    std::int64_t domain_value = 0;
    std::int64_t sample_count = 0;

    if (_table)
    {
        if (size >= sizeof(streaming_protocol::linear_payload))
            _table->update(*static_cast<const streaming_protocol::linear_payload *>(data));
    }

    else if (_is_explicit)
    {
        if (_sample_size)
            sample_count = size / _sample_size;

        auto domain_table = _domain_table.lock();
        if (domain_table)
            domain_value = domain_table->value_at(_value_index);

        _value_index += sample_count;

        if (domain_table)
            domain_table->drive_to(_value_index);
    }

    else
    {
        if (size >= _sample_size)
        {
            sample_count = 1;
            size = _sample_size;
        }

        if (auto domain_table = _domain_table.lock(); domain_table)
            domain_value = domain_table->driven_value();
    }

    on_data_received(
        domain_value,
        sample_count,
        data,
        size);
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
    _sample_size = metadata().sample_size();

    if (_metadata.rule() == rule_types::linear_rule)
    {
        if (_table)
            _table->update(_metadata);
        else
            _table = std::make_shared<linear_table>(_metadata);
    }

    else
    {
        _table.reset();
    }

    _is_explicit = _metadata.rule() == rule_types::explicit_rule;
    _value_index = _metadata.value_index().value_or(_value_index);

    auto table_id = _metadata.table_id();

    if (!table_id.empty() && table_id != id())
    {
        _domain_signal = on_signal_sought(table_id).value_or(nullptr);
        if (_domain_signal)
            _domain_table = _domain_signal->_table;
        else
            _domain_table.reset();
    }

    else
    {
        _domain_signal.reset();
        _domain_table.reset();
    }

    on_metadata_changed();
}
