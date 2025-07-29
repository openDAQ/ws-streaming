#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

#include <boost/asio.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>
#include <ws-streaming/transport/http_client_servicer.hpp>
#include <ws-streaming/transport/server.hpp>

using namespace std::placeholders;

wss::transport::server::server(boost::asio::any_io_executor executor)
    : _executor(executor)
{
    add_listener(detail::streaming_protocol::DEFAULT_WEBSOCKET_PORT);
    add_listener(detail::streaming_protocol::DEFAULT_CONTROL_PORT);
}

void wss::transport::server::run()
{
    for (const auto& listener : _listeners)
    {
        listener.l->run();
    }
}

void wss::transport::server::stop()
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

void wss::transport::server::add_signal(local_signal& signal)
{
    if (_signals.emplace(&signal).second)
        for (const auto& c : _clients)
            c.connection->add_signal(signal);
}

void wss::transport::server::remove_signal(local_signal& signal)
{
    if (_signals.erase(&signal))
        for (const auto& c : _clients)
            c.connection->remove_signal(signal);
}

void wss::transport::server::add_listener(std::uint16_t port)
{
    add_listener(
        std::make_shared<listener<>>(
            _executor,
            boost::asio::ip::tcp::endpoint({}, port)));
}

void wss::transport::server::add_listener(
    std::shared_ptr<listener<>> listener)
{
    _listeners.emplace_back(
        listener,
        listener->on_accept.connect(std::bind(&server::on_listener_accept, this, _1)));
}

void wss::transport::server::on_listener_accept(
    boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
        return;

    std::cout << "accepted connection" << std::endl;
    auto client = std::make_shared<http_client_servicer>(std::move(socket));
    _sessions.emplace_back(
        client,
        client->on_control_request.connect(std::bind(&server::on_servicer_control_request, this, client, _1)),
        client->on_websocket_upgrade.connect(std::bind(&server::on_servicer_websocket_upgrade, this, client, _1)),
        client->on_closed.connect(std::bind(&server::on_servicer_closed, this, client, _1)));

    client->run();
}

nlohmann::json wss::transport::server::on_servicer_control_request(
    const std::shared_ptr<http_client_servicer>& servicer,
    const nlohmann::json& request)
{
    std::cout << "control request: " << request.dump() << std::endl;
    return nlohmann::json::object();    
}

void wss::transport::server::on_servicer_websocket_upgrade(
    const std::shared_ptr<http_client_servicer>& servicer,
    boost::asio::ip::tcp::socket& socket)
{
    std::cout << "websocket connection!" << std::endl;

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

void wss::transport::server::on_servicer_closed(
    const std::shared_ptr<http_client_servicer>& servicer,
    const boost::system::error_code& ec)
{
    std::cout << "server removing servicer from list (disconnect ec " << ec << ')' << std::endl;

    _sessions.remove_if([&](const client_entry& entry)
    {
        return entry.client == servicer;
    });
}

void wss::transport::server::on_connection_disconnected(
    const std::shared_ptr<wss::connection>& connection)
{
    std::cout << "server removing peer from list" << std::endl;

    _clients.remove_if([&](const connected_client& client)
    {
        return client.connection == connection;
    });
}
