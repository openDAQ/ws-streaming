#include <functional>
#include <iostream>
#include <memory>
#include <utility>

#include <boost/asio.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/http_client_servicer.hpp>
#include <ws-streaming/peer.hpp>
#include <ws-streaming/server.hpp>
#include <ws-streaming/streaming_protocol.hpp>

using namespace std::placeholders;

wss::server::server(boost::asio::any_io_executor executor)
    : strand(boost::asio::make_strand(executor))
{
    add_listener(streaming_protocol::DEFAULT_WEBSOCKET_PORT);
    add_listener(streaming_protocol::DEFAULT_CONTROL_PORT);
}

void wss::server::run()
{
    for (const auto& listener : listeners)
    {
        listener.l->run();
    }
}

void wss::server::stop()
{
    for (const auto& listener : listeners)
        listener.l->stop();
    listeners.clear();

    for (const auto& peer : peers)
        peer.peer->stop();
    peers.clear();

    for (const auto& session : sessions)
        session.client->stop();
    sessions.clear();
}

void wss::server::debug_broadcast()
{
    for (const auto& peer : peers)
        peer.peer->send_metadata(0, "hello", {{"foo", "bar"}});
}

void wss::server::add_listener(std::uint16_t port)
{
    add_listener(
        std::make_shared<listener<>>(
            strand,
            boost::asio::ip::tcp::endpoint({}, port)));
}

void wss::server::add_listener(std::shared_ptr<listener<>> listener)
{
    listeners.emplace_back(
        listener,
        listener->on_accept.connect(std::bind(&server::on_listener_accept, this, _1)));
}

void wss::server::on_listener_accept(boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
        return;

    std::cout << "accepted connection" << std::endl;
    auto client = std::make_shared<http_client_servicer>(std::move(socket));
    sessions.emplace_back(
        client,
        client->on_control_request.connect(std::bind(&server::on_servicer_control_request, this, client, _1)),
        client->on_websocket_upgrade.connect(std::bind(&server::on_servicer_websocket_upgrade, this, client, _1)),
        client->on_closed.connect(std::bind(&server::on_servicer_closed, this, client, _1)));

    client->run();
}

nlohmann::json wss::server::on_servicer_control_request(
    const std::shared_ptr<http_client_servicer>& servicer,
    const nlohmann::json& request)
{
    std::cout << "control request: " << request.dump() << std::endl;
    return nlohmann::json::object();    
}

void wss::server::on_servicer_websocket_upgrade(
    const std::shared_ptr<http_client_servicer>& servicer,
    boost::asio::ip::tcp::socket& socket)
{
    std::cout << "websocket connection!" << std::endl;

    auto peer = std::make_shared<wss::peer>(
        std::move(socket),
        false);

    peers.emplace_back(
        peer,
        peer->on_data_received.connect(std::bind(&server::on_peer_data_received, this, peer, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
        peer->on_metadata_received.connect(std::bind(&server::on_peer_metadata_received, this, peer, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
        peer->on_closed.connect(std::bind(&server::on_peer_closed, this, peer, std::placeholders::_1)));

    peer->run();
}

void wss::server::on_servicer_closed(
    const std::shared_ptr<http_client_servicer>& servicer,
    const boost::system::error_code& ec)
{
    std::cout << "server removing servicer from list (disconnect ec " << ec << ')' << std::endl;

    sessions.remove_if([&](const client_entry& entry)
    {
        return entry.client == servicer;
    });
}

void wss::server::on_peer_data_received(
    const std::shared_ptr<peer>& peer,
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    std::cout << "server received data from peer (" << signo << "): " << size << std::endl;
}

void wss::server::on_peer_metadata_received(
    const std::shared_ptr<peer>& peer,
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    std::cout << "server received metadata from peer (" << method << "): " << params.dump() << std::endl;
}

void wss::server::on_peer_closed(
    const std::shared_ptr<peer>& peer,
    const boost::system::error_code& ec)
{
    std::cout << "server removing peer from list (disconnect ec " << ec << ')' << std::endl;

    peers.remove_if([&](const peer_entry& entry)
    {
        return entry.peer == peer;
    });
}
