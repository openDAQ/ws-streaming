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
#include <ws-streaming/detail/http_command_interface.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/semver.hpp>
#include <ws-streaming/transport/peer.hpp>

using namespace std::placeholders;

wss::connection::connection(
        const std::string& hostname,
        boost::asio::ip::tcp::socket&& socket,
        bool is_client)
    : _hostname(hostname)
    , _is_client(is_client)
    , _peer(std::make_shared<transport::peer>(std::move(socket), is_client))
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
    std::cout << "calling _peer->stop()" << std::endl;
    _peer->stop();
}

void wss::connection::do_hello()
{
    _peer->send_metadata(
        0,
        "",
        {});
}

void wss::connection::on_peer_data_received(
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    auto it = _signals_by_signo.find(signo);
    if (it == _signals_by_signo.end())
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
}

void wss::connection::on_peer_closed(
    const boost::system::error_code& ec)
{
    std::cout << "connection::on_peer_closed" << std::endl;

    _on_peer_data_received.disconnect();
    _on_peer_metadata_received.disconnect();
    _on_peer_closed.disconnect();

    std::map<std::string, signal_entry> old_signals;
    std::swap(old_signals, _signals_by_id);
    _signals_by_signo.clear();

    for (const auto& signal : old_signals)
        signal.second.signal->detach();

    for (const auto& signal : old_signals)
        on_unavailable(signal.second.signal);

    on_disconnected();
}

void wss::connection::on_signal_subscribe_requested(const std::string& signal_id)
{
    std::cout << "someone requested signal subscribe to " << signal_id << std::endl;
    if (!_command_interface)
        return; // @todo XXX TODO

    _command_interface->async_request(_stream_id + ".subscribe", { signal_id },
        [](const boost::system::error_code& ec, const nlohmann::json& response)
        {
            std::cout << "subscribe control request done: " << ec << " " << response.dump() << std::endl;
        });
}

void wss::connection::on_signal_unsubscribe_requested(const std::string& signal_id)
{
    std::cout << "someone requested signal unsubscribe from " << signal_id << std::endl;
    if (!_command_interface)
        return; // @todo XXX TODO

    _command_interface->async_request(_stream_id + ".unsubscribe", { signal_id },
        [](const boost::system::error_code& ec, const nlohmann::json& response)
        {
            std::cout << "unsubscribe control request done: " << ec << " " << response.dump() << std::endl;
        });
}

void wss::connection::dispatch_metadata(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    auto it = _signals_by_signo.find(signo);
    if (it == _signals_by_signo.end())
        return;

    it->second->signal->handle_metadata(method, params);
}

void wss::connection::handle_api_version(const nlohmann::json& params)
{
    if (params.is_object()
            && params.contains("version")
            && params["version"].is_string())
        _api_version = detail::semver::try_parse(params["version"]).value_or(detail::semver());
}

void wss::connection::handle_init(const nlohmann::json& params)
{
    std::cout << "a" << std::endl;
    if (!params.is_object())
        return;

    if (params.contains("streamId") && params["streamId"].is_string())
        _stream_id = params["streamId"];

    std::cout << "b" << std::endl;
    if (params.contains("commandInterfaces")
        && params["commandInterfaces"].is_object())
    {
        std::cout << "c" << std::endl;
        const auto& ci = params["commandInterfaces"];
        if (ci.contains("jsonrpc-http")
            && ci["jsonrpc-http"].is_object()
            && ci["jsonrpc-http"].contains("httpMethod")
            && ci["jsonrpc-http"]["httpMethod"].is_string()
            && ci["jsonrpc-http"].contains("httpPath")
            && ci["jsonrpc-http"]["httpPath"].is_string()
            && ci["jsonrpc-http"].contains("httpVersion")
            && ci["jsonrpc-http"]["httpVersion"].is_string()
            && ci["jsonrpc-http"].contains("port")
            && (ci["jsonrpc-http"]["port"].is_string() || ci["jsonrpc-http"]["port"].is_number_integer()))
        {
            std::uint16_t port;
            if (ci["jsonrpc-http"]["port"].is_number_integer())
                port = ci["jsonrpc-http"]["port"].is_number_integer();
            else
                port = std::strtoul(std::string(ci["jsonrpc-http"]["port"]).c_str(), nullptr, 10);

            std::cout << "d" << std::endl;
            _command_interface = std::make_unique<detail::http_command_interface>(
                _peer->socket().get_executor(),
                _hostname,
                port,
                ci["jsonrpc-http"]["httpMethod"],
                ci["jsonrpc-http"]["httpPath"],
                ci["jsonrpc-http"]["httpVersion"]);
            std::cout << "e" << std::endl;
        }
    }

    if (_is_client && _api_version >= detail::semver(2, 0, 0))
        do_hello();
}

void wss::connection::handle_available(const nlohmann::json& params)
{
    if (!params.is_object() || !params.contains("signalIds") || !params["signalIds"].is_array())
        return;

    for (const auto& id : params["signalIds"])
    {
        if (!id.is_string())
            continue;

        auto it = _signals_by_id.find(id);
        if (it != _signals_by_id.end())
            continue;

        auto& entry = _signals_by_id.emplace(id, id).first->second;

        entry.on_subscribe_requested = entry.signal->on_subscribe_requested.connect(
            std::bind(&connection::on_signal_subscribe_requested, this, id));

        entry.on_unsubscribe_requested = entry.signal->on_unsubscribe_requested.connect(
            std::bind(&connection::on_signal_unsubscribe_requested, this, id));

        on_available(entry.signal);
    }
}

void wss::connection::handle_subscribe(unsigned signo, const nlohmann::json& params)
{
    if (!params.is_object()
            || !params.contains("signalId")
            || !params["signalId"].is_string())
        return;

    auto it = _signals_by_id.find(params["signalId"]);
    if (it == _signals_by_id.end())
        return;

    _signals_by_signo[signo] = &it->second;

    dispatch_metadata(signo, "subscribe", params);
}

void wss::connection::handle_unsubscribe(unsigned signo, const nlohmann::json& params)
{
    dispatch_metadata(signo, "unsubscribe", params);

    _signals_by_signo.erase(signo);
}

void wss::connection::handle_unavailable(const nlohmann::json& params)
{
    if (!params.is_object() || !params.contains("signalIds") || !params["signalIds"].is_array())
        return;

    for (const auto& id : params["signalIds"])
    {
        if (!id.is_string())
            continue;

        auto it = _signals_by_id.find(id);
        if (it == _signals_by_id.end())
            continue;

        auto signal = it->second.signal;
        _signals_by_id.erase(it);
        _signals_by_signo.erase(signal->signo());

        signal->detach();
        on_unavailable(signal);
    }
}
