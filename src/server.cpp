#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/json_rpc_exception.hpp>
#include <ws-streaming/server.hpp>
#include <ws-streaming/detail/http_client_servicer.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

using namespace std::placeholders;

wss::server::server(boost::asio::any_io_executor executor)
    : _executor(executor)
{
}

void wss::server::add_listener(std::uint16_t port, bool make_command_interface)
{
    add_listener(
        std::make_shared<listener<>>(
            _executor,
            boost::asio::ip::tcp::endpoint({}, port)));

    if (make_command_interface)
        _command_interface_port = port;
}

void wss::server::add_listener(
    std::shared_ptr<listener<>> listener)
{
    _listeners.emplace_back(
        listener,
        listener->on_accept.connect(
            std::bind(
                &server::on_listener_accept,
                this,
                _1)));
}

void wss::server::add_default_listeners()
{
    add_listener(detail::streaming_protocol::DEFAULT_WEBSOCKET_PORT);
    add_listener(detail::streaming_protocol::DEFAULT_COMMAND_INTERFACE_PORT, true);
}

void wss::server::run()
{
    for (const auto& listener : _listeners)
    {
        listener.l->run();
    }
}

void wss::server::close()
{
    if (_closed.exchange(true))
        return;

    for (const auto& listener : _listeners)
        listener.l->stop();
    _listeners.clear();

    for (const auto& client : _clients)
        client.connection->close();
    _clients.clear();

    for (const auto& session : _sessions)
        session.client->stop();
    _sessions.clear();

    on_closed({});
}

void wss::server::add_local_signal(local_signal& signal)
{
    if (_signals.emplace(&signal).second)
    {
        _ordered_signals.push_back(&signal);
        for (const auto& c : _clients)
            c.connection->add_local_signal(signal);
    }
}

void wss::server::remove_local_signal(local_signal& signal)
{
    if (_signals.erase(&signal))
    {
        _ordered_signals.remove(&signal);
        for (const auto& c : _clients)
            c.connection->remove_local_signal(signal);
    }
}

void wss::server::on_listener_accept(
    boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
        return;

    auto client = std::make_shared<detail::http_client_servicer>(std::move(socket));
    _sessions.emplace_back(
        client,
        client->on_command_interface_request.connect(std::bind(&server::on_servicer_command_interface_request, this, client, _1, _2)),
        client->on_websocket_upgrade.connect(std::bind(&server::on_servicer_websocket_upgrade, this, client, _1)),
        client->on_closed.connect(std::bind(&server::on_servicer_closed, this, client, _1)));

    client->run();
}

nlohmann::json wss::server::on_servicer_command_interface_request(
    const std::shared_ptr<detail::http_client_servicer>& servicer,
    const std::string& method,
    const nlohmann::json& params)
{
    std::string stream_id = method.substr(0, method.rfind('.'));

    auto it = std::find_if(
        _clients.begin(),
        _clients.end(),
        [&](detail::connected_client& client)
        {
            return client.connection->local_stream_id() == stream_id;
        });

    if (it != _clients.end())
        return it->connection->do_command_interface(method, params);

    throw json_rpc_exception(
        json_rpc_exception::server_error,
        "no client with stream id " + stream_id);
}

void wss::server::on_servicer_websocket_upgrade(
    const std::shared_ptr<detail::http_client_servicer>& servicer,
    boost::asio::ip::tcp::socket& socket)
{
    auto connection = std::make_shared<wss::connection>(
        std::move(socket),
        false);

    if (_command_interface_port)
        connection->register_external_command_interface(
            "jsonrpc-http",
            {
                { "httpMethod", "POST" },
                { "httpPath", "/" },
                { "httpVersion", "1.1" },
                { "port", std::to_string(_command_interface_port) }
            });

    for (const auto& signal : _ordered_signals)
        connection->add_local_signal(*signal);

    auto& entry = _clients.emplace_back(connection);

    entry.on_available = connection->on_available.connect(
        std::bind(
            &server::on_connection_available,
            this,
            connection,
            _1));

    entry.on_unavailable = connection->on_unavailable.connect(
        std::bind(
            &server::on_connection_unavailable,
            this,
            connection,
            _1));

    entry.on_disconnected = connection->on_disconnected.connect(
        std::bind(
            &server::on_connection_disconnected,
            this,
            connection,
            _1));

    connection->run();

    on_client_connected(connection);
}

void wss::server::on_servicer_closed(
    const std::shared_ptr<detail::http_client_servicer>& servicer,
    const boost::system::error_code& ec)
{
    _sessions.remove_if([&](const client_entry& entry)
    {
        return entry.client == servicer;
    });
}

void wss::server::on_connection_available(
    connection_ptr connection,
    remote_signal_ptr signal)
{
    on_available(connection, signal);
}

void wss::server::on_connection_unavailable(
    connection_ptr connection,
    remote_signal_ptr signal)
{
    on_unavailable(connection, signal);
}

void wss::server::on_connection_disconnected(
    connection_ptr connection,
    const boost::system::error_code& ec)
{
    _clients.remove_if([&](const detail::connected_client& client)
    {
        return client.connection == connection;
    });

    on_client_disconnected(connection, ec);
}
