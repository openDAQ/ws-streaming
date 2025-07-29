#include <cstddef>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>

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
    if (_is_subscribed)
        on_data_received();
}

void wss::detail::remote_signal_impl::handle_metadata(
    const std::string& method,
    const nlohmann::json& params)
{
    std::cout << "remote signal metadata: " << method << ": " << params.dump() << std::endl;

    if (method == "subscribe")
    {
        if (!_is_subscribed)
        {
            _is_subscribed = true;
            on_subscribed();
        }
    }

    else if (method == "unsubscribe")
    {
        if (_is_subscribed)
        {
            _is_subscribed = false;
            on_unsubscribed();
        }
    }
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
}
