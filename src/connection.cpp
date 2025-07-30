#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <utility>

#include <boost/asio/ip/tcp.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/command_interface_client_factory.hpp>
#include <ws-streaming/detail/in_band_command_interface_client.hpp>
#include <ws-streaming/detail/json_rpc_exception.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/semver.hpp>
#include <ws-streaming/transport/peer.hpp>

using namespace std::placeholders;

wss::connection::connection(
        const std::string& hostname,
        boost::asio::ip::tcp::socket&& socket,
        bool is_client)
    : _hostname{hostname}
    , _is_client{is_client}
    , _peer{std::make_shared<transport::peer>(std::move(socket), is_client)}
    , _local_stream_id{_peer->socket().remote_endpoint().address().to_string()
        + ":" + std::to_string(_peer->socket().remote_endpoint().port())}
{
    _on_peer_data_received = _peer->on_data_received.connect(std::bind(&connection::on_peer_data_received, this, _1, _2, _3));
    _on_peer_metadata_received = _peer->on_metadata_received.connect(std::bind(&connection::on_peer_metadata_received, this, _1, _2, _3));
    _on_peer_closed = _peer->on_closed.connect(std::bind(&connection::on_peer_closed, this, _1));
}

void wss::connection::run()
{
    _peer->run();

    if (!_is_client)
        do_hello();
}

void wss::connection::run(const void *data, std::size_t size)
{
    _peer->run(data, size);

    if (!_is_client)
        do_hello();
}

void wss::connection::stop()
{
    _peer->stop();
}

void wss::connection::add_signal(local_signal& signal)
{
    auto [signo, added] = add_local_signal(signal);

    if (added)
        _peer->send_metadata(signo, "available", { { "signalIds", { signal.id() } } });
}

void wss::connection::remove_signal(local_signal& signal)
{
    unsigned signo = remove_local_signal(signal);

    if (signo)
        _peer->send_metadata(signo, "unavailable", { { "signalIds", { signal.id() } } });
}

void wss::connection::do_hello()
{
    _peer->send_metadata(
        0,
        "apiVersion",
        {
            { "version", "2.0.0" }
        });

    _peer->send_metadata(
        0,
        "init",
        {
            { "streamId", _local_stream_id },
            { "commandInterfaces", { { "jsonrpc", nullptr } } }
        });

    auto signal_ids = nlohmann::json::array();

    for (const auto& signal : local_signals())
        signal_ids.emplace_back(signal.signal.id());

    if (!signal_ids.empty())
        _peer->send_metadata(
            0,
            "available",
            {
                { "signalIds", signal_ids }
            });
}

void wss::connection::on_peer_data_received(
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    auto it = _remote_signals_by_signo.find(signo);
    if (it == _remote_signals_by_signo.end())
        return;

    it->second->signal->handle_data(data, size);
}

void wss::connection::on_peer_metadata_received(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    std::cout << "(connection) metadata (" << signo << ") " << method << ": " << params.dump() << std::endl;

    if (method == "subscribe")
        handle_subscribe(signo, params);
    else if (method == "unsubscribe")
        handle_unsubscribe(signo, params);
    else if (signo)
        dispatch_metadata(signo, method, params);
    else if (method == "apiVersion")
        handle_api_version(params);
    else if (method == "init")
        handle_init(params);
    else if (method == "available")
        handle_available(params);
    else if (method == "unavailable")
        handle_available(params);
    else if (method == "request")
        handle_command_interface_request(params);
    else if (method == "response")
        handle_command_interface_response(params);
}

void wss::connection::on_peer_closed(
    const boost::system::error_code& ec)
{
    std::cout << "connection::on_peer_closed" << std::endl;

    _on_peer_data_received.disconnect();
    _on_peer_metadata_received.disconnect();
    _on_peer_closed.disconnect();

    std::map<std::string, signal_entry> old_signals;
    std::swap(old_signals, _remote_signals_by_id);
    _remote_signals_by_signo.clear();

    for (const auto& signal : old_signals)
        signal.second.signal->detach();

    for (const auto& signal : old_signals)
        on_unavailable(signal.second.signal);

    clear_local_signals();

    on_disconnected();
}

void wss::connection::on_local_signal_metadata_changed(
    unsigned signo,
    const nlohmann::json& metadata)
{
    std::cout << "on_local_signal_metadata_changed" << std::endl;
    _peer->send_metadata(signo, "signal", metadata);
}

void wss::connection::on_local_signal_data(
    unsigned signo,
    const void *data,
    std::size_t size)
{
    std::cout << "on_local_signal_data" << std::endl;
    _peer->send_data(
        signo,
        boost::asio::const_buffer{data, size});
}

void wss::connection::on_signal_subscribe_requested(
    const std::string& signal_id)
{
    std::cout << "someone requested signal subscribe to " << signal_id << std::endl;
    if (!_command_interface_client)
        return; // @todo XXX TODO

    _command_interface_client->async_request(_remote_stream_id + ".subscribe", { signal_id },
        [](const boost::system::error_code& ec, const nlohmann::json& response)
        {
            std::cout << "subscribe command interface request done: " << ec << " " << response.dump() << std::endl;
        });
}

void wss::connection::on_signal_unsubscribe_requested(
    const std::string& signal_id)
{
    std::cout << "someone requested signal unsubscribe from " << signal_id << std::endl;
    if (!_command_interface_client)
        return; // @todo XXX TODO

    _command_interface_client->async_request(_remote_stream_id + ".unsubscribe", { signal_id },
        [](const boost::system::error_code& ec, const nlohmann::json& response)
        {
            std::cout << "unsubscribe command interface request done: " << ec << " " << response.dump() << std::endl;
        });
}

void wss::connection::dispatch_metadata(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    auto it = _remote_signals_by_signo.find(signo);
    if (it == _remote_signals_by_signo.end())
        return;

    it->second->signal->handle_metadata(method, params);
}

void wss::connection::handle_api_version(
    const nlohmann::json& params)
{
    if (params.is_object()
            && params.contains("version")
            && params["version"].is_string())
        _api_version = detail::semver::try_parse(params["version"]).value_or(detail::semver());
}

void wss::connection::handle_init(
    const nlohmann::json& params)
{
    if (!params.is_object())
        return;

    if (params.contains("streamId") && params["streamId"].is_string())
        _remote_stream_id = params["streamId"];

    if (params.contains("commandInterfaces"))
        _command_interface_client = detail::command_interface_client_factory::create_client(
            params["commandInterfaces"],
            _peer);

    if (_is_client && _api_version >= detail::semver(2, 0, 0))
        do_hello();
}

void wss::connection::handle_available(
    const nlohmann::json& params)
{
    if (!params.is_object() || !params.contains("signalIds") || !params["signalIds"].is_array())
        return;

    for (const auto& id : params["signalIds"])
    {
        if (!id.is_string())
            continue;

        auto it = _remote_signals_by_id.find(id);
        if (it != _remote_signals_by_id.end())
            continue;

        auto& entry = _remote_signals_by_id.emplace(id, id).first->second;

        entry.on_subscribe_requested = entry.signal->on_subscribe_requested.connect(
            std::bind(&connection::on_signal_subscribe_requested, this, id));

        entry.on_unsubscribe_requested = entry.signal->on_unsubscribe_requested.connect(
            std::bind(&connection::on_signal_unsubscribe_requested, this, id));

        on_available(entry.signal);
    }
}

void wss::connection::handle_subscribe(
    unsigned signo,
    const nlohmann::json& params)
{
    if (!params.is_object()
            || !params.contains("signalId")
            || !params["signalId"].is_string())
        return;

    auto it = _remote_signals_by_id.find(params["signalId"]);
    if (it == _remote_signals_by_id.end())
        return;

    _remote_signals_by_signo[signo] = &it->second;

    dispatch_metadata(signo, "subscribe", params);
}

void wss::connection::handle_unsubscribe(
    unsigned signo,
    const nlohmann::json& params)
{
    dispatch_metadata(signo, "unsubscribe", params);

    _remote_signals_by_signo.erase(signo);
}

void wss::connection::handle_unavailable(
    const nlohmann::json& params)
{
    if (!params.is_object()
            || !params.contains("signalIds")
            || !params["signalIds"].is_array())
        return;

    for (const auto& id : params["signalIds"])
    {
        if (!id.is_string())
            continue;

        auto it = _remote_signals_by_id.find(id);
        if (it == _remote_signals_by_id.end())
            continue;

        auto signal = it->second.signal;
        _remote_signals_by_id.erase(it);
        _remote_signals_by_signo.erase(signal->signo());

        signal->detach();
        on_unavailable(signal);
    }
}

void wss::connection::handle_command_interface_request(const nlohmann::json& params)
{
    nlohmann::json result;
    nlohmann::json error;

    try
    {
        if (!params.is_object()
                || !params.contains("method")
                || !params["method"].is_string())
            throw detail::json_rpc_exception(
                detail::json_rpc_exception::invalid_request,
                "invalid request object");

        std::string method = params["method"];

        if (method == _local_stream_id + ".subscribe")
            result = do_command_interface_subscribe(params.value<nlohmann::json>("params", nullptr));

        else if (method == _local_stream_id + ".unsubscribe")
            result = do_command_interface_unsubscribe(params.value<nlohmann::json>("params", nullptr));

        else
            throw detail::json_rpc_exception(
                detail::json_rpc_exception::method_not_found,
                "method not found");
    }

    catch (const detail::json_rpc_exception& ex)
    {
        error = ex.json();
    }

    nlohmann::json response_params
    {
        { "jsonrpc", "2.0" },
        { "id", params.value<nlohmann::json>("id", nullptr) }
    };

    if (!error.is_null())
        response_params["error"] = error;
    else response_params["result"] = result;

    _peer->send_metadata(
        0,
        "response",
        response_params);
}

nlohmann::json wss::connection::do_command_interface_subscribe(const nlohmann::json& params)
{
    auto subscribe = [&](const std::string& signal_id)
    {
        auto *signal = find_local_signal(signal_id);
        if (!signal || signal->is_subscribed)
            return false;

        signal->is_subscribed = true;

        signal->on_data = signal->signal.on_data.connect(
            std::bind(
                &connection::on_local_signal_data,
                shared_from_this(),
                signal->signo,
                _1,
                _2));

        signal->on_metadata_changed = signal->signal.on_metadata_changed.connect(
            std::bind(
                &connection::on_local_signal_metadata_changed,
                shared_from_this(),
                signal->signo,
                _1));

        _peer->send_metadata(
            signal->signo,
            "subscribe",
            {
                { "signalId", signal->signal.id() }
            });

        _peer->send_metadata(
            signal->signo,
            "signal",
            signal->signal.metadata());

        return true;
    };

    if (params.is_string())
    {
        if (!subscribe(params))
            throw detail::json_rpc_exception(
                detail::json_rpc_exception::server_error,
                "failed to subscribe signal");

        return true;
    }

    else if (params.is_array())
    {
        auto results = nlohmann::json::array();

        for (const auto& signal_id : params)
            results.push_back(signal_id.is_string() && subscribe(signal_id));

        return results;
    }

    else
        throw detail::json_rpc_exception(
            detail::json_rpc_exception::invalid_params,
            "params must be a signal ID or an array of signal IDs");
}

nlohmann::json wss::connection::do_command_interface_unsubscribe(const nlohmann::json& params)
{
    auto unsubscribe = [&](const std::string& signal_id)
    {
        auto *signal = find_local_signal(signal_id);
        if (!signal || !signal->is_subscribed)
            return false;

        signal->is_subscribed = false;
        signal->on_data.disconnect();
        signal->on_metadata_changed.disconnect();

        return true;
    };

    if (params.is_string())
    {
        if (!unsubscribe(params))
            throw detail::json_rpc_exception(
                detail::json_rpc_exception::server_error,
                "failed to unsubscribe signal");

        return true;
    }

    else if (params.is_array())
    {
        auto results = nlohmann::json::array();

        for (const auto& signal_id : params)
            results.push_back(signal_id.is_string() && unsubscribe(signal_id));

        return results;
    }

    else
        throw detail::json_rpc_exception(
            detail::json_rpc_exception::invalid_params,
            "params must be a signal ID or an array of signal IDs");
}

void wss::connection::handle_command_interface_response(const nlohmann::json& params)
{
    if (auto *client = dynamic_cast<detail::in_band_command_interface_client *>(_command_interface_client.get()); client)
        client->handle_response(params);
}
