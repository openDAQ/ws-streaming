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

#if WS_STREAMING_ENABLE_TLS
#include <ws-streaming/detail/tls.hpp>
#endif

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
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)));

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

#if WS_STREAMING_ENABLE_TLS

void wss::server::add_tls_listener(
    std::uint16_t port,
    const std::string& cert_file,
    const std::string& key_file,
    const std::string& ca_file,
    bool make_command_interface)
{
    _ssl_context = std::make_unique<boost::asio::ssl::context>(
        detail::tls::make_server_tls_context(cert_file, key_file, ca_file));

    auto listener = std::make_shared<wss::listener<>>(
        _executor,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port));

    _listeners.emplace_back(
        listener,
        listener->on_accept.connect(
            std::bind(&server::on_tls_listener_accept, this, _1)));

    if (make_command_interface)
        _command_interface_port = port;
}

#endif

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

    start_session(
        std::make_shared<detail::http_client_servicer>(std::move(socket)));
}

#if WS_STREAMING_ENABLE_TLS

void wss::server::on_tls_listener_accept(
    boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
        return;

    start_session(
        std::make_shared<detail::https_client_servicer>(std::move(socket), *_ssl_context));
}

#endif

nlohmann::json wss::server::on_servicer_command_interface_request(
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

void wss::server::on_servicer_closed(
    std::weak_ptr<detail::http_servicer_base> servicer,
    const boost::system::error_code& /*ec*/)
{
    auto servicer_base_prt = servicer.lock();
    if (!servicer_base_prt)
        return;

    _sessions.remove_if([&](const client_entry& entry)
    {
        return entry.client == servicer_base_prt;
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
