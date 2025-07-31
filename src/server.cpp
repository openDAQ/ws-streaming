#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/server.hpp>
#include <ws-streaming/detail/http_client_servicer.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

using namespace std::placeholders;

wss::server::server(boost::asio::any_io_executor executor)
    : _executor(executor)
{
    add_listener(detail::streaming_protocol::DEFAULT_WEBSOCKET_PORT);
    add_listener(detail::streaming_protocol::DEFAULT_COMMAND_INTERFACE_PORT);
}

void wss::server::run()
{
    for (const auto& listener : _listeners)
    {
        listener.l->run();
    }
}

void wss::server::stop()
{
    for (const auto& listener : _listeners)
        listener.l->stop();
    _listeners.clear();

    for (const auto& client : _clients)
        client.connection->stop();
    _clients.clear();

    for (const auto& session : _sessions)
        session.client->stop();
    _sessions.clear();
}

void wss::server::add_signal(local_signal& signal)
{
    if (_signals.emplace(&signal).second)
        for (const auto& c : _clients)
            c.connection->add_signal(signal);
}

void wss::server::remove_signal(local_signal& signal)
{
    if (_signals.erase(&signal))
        for (const auto& c : _clients)
            c.connection->remove_signal(signal);
}

void wss::server::add_listener(std::uint16_t port)
{
    add_listener(
        std::make_shared<listener<>>(
            _executor,
            boost::asio::ip::tcp::endpoint({}, port)));
}

void wss::server::add_listener(
    std::shared_ptr<listener<>> listener)
{
    _listeners.emplace_back(
        listener,
        listener->on_accept.connect(std::bind(&server::on_listener_accept, this, _1)));
}

void wss::server::on_listener_accept(
    boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
        return;

    auto client = std::make_shared<detail::http_client_servicer>(std::move(socket));
    _sessions.emplace_back(
        client,
        client->on_command_interface_request.connect(std::bind(&server::on_servicer_command_interface_request, this, client, _1)),
        client->on_websocket_upgrade.connect(std::bind(&server::on_servicer_websocket_upgrade, this, client, _1)),
        client->on_closed.connect(std::bind(&server::on_servicer_closed, this, client, _1)));

    client->run();
}

nlohmann::json wss::server::on_servicer_command_interface_request(
    const std::shared_ptr<detail::http_client_servicer>& servicer,
    const nlohmann::json& request)
{
    return nlohmann::json::object();    
}

void wss::server::on_servicer_websocket_upgrade(
    const std::shared_ptr<detail::http_client_servicer>& servicer,
    boost::asio::ip::tcp::socket& socket)
{
    auto connection = std::make_shared<wss::connection>(
        socket.remote_endpoint().address().to_string(),
        std::move(socket),
        false);

    for (const auto& signal : _signals)
        connection->add_signal(*signal);

    _clients.emplace_back(
        connection,
        connection->on_disconnected.connect(
            std::bind(
                &server::on_connection_disconnected,
                this,
                connection)));

    connection->run();
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

void wss::server::on_connection_disconnected(
    const std::shared_ptr<wss::connection>& connection)
{
    _clients.remove_if([&](const connected_client& client)
    {
        return client.connection == connection;
    });
}
