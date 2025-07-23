#pragma once

#include <list>
#include <memory>

#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/listener.hpp>
#include <ws-streaming/peer.hpp>
#include <ws-streaming/session.hpp>

namespace wss
{
    class server
    {
        public:

            server(boost::asio::any_io_executor executor);

            void run();
            void stop();

            // boost::signals2::signal<void()> on_metadata;
            // boost::signals2::signal<void()> on_data;
            // boost::signals2::signal<nlohmann::json(const nlohmann::json& request)> on_control_request;

            void debug_broadcast();

        private:

            void add_listener(std::uint16_t port);
            void add_listener(std::shared_ptr<listener> listener);

            void on_listener_accept(boost::asio::ip::tcp::socket& socket);
            nlohmann::json on_control_request(const nlohmann::json& request);
            void on_websocket_connection(boost::asio::ip::tcp::socket& socket);
            void on_peer_disconnected(const std::shared_ptr<peer>& peer);

            struct listener_entry
            {
                listener_entry(
                        std::shared_ptr<listener> l,
                        boost::signals2::scoped_connection connection)
                    : l(l)
                    , connection(std::move(connection))
                {
                }

                std::shared_ptr<listener> l;
                boost::signals2::scoped_connection connection;
            };

            struct client_entry
            {
                client_entry(
                        std::shared_ptr<session> client,
                        boost::signals2::scoped_connection on_websocket_connection,
                        boost::signals2::scoped_connection on_control_request)
                    : client(client)
                    , on_websocket_connection(std::move(on_websocket_connection))
                    , on_control_request(std::move(on_control_request))
                {
                }

                std::shared_ptr<session> client;
                boost::signals2::scoped_connection on_websocket_connection;
                boost::signals2::scoped_connection on_control_request;
            };

            struct peer_entry
            {
                peer_entry(
                        std::shared_ptr<wss::peer> peer,
                        boost::signals2::scoped_connection on_disconnected)
                    : peer(peer)
                    , on_disconnected(std::move(on_disconnected))
                {
                }

                std::shared_ptr<wss::peer> peer;
                boost::signals2::scoped_connection on_disconnected;
            };

            boost::asio::strand<boost::asio::any_io_executor> strand;
            std::list<listener_entry> listeners;
            std::list<client_entry> sessions;
            std::list<peer_entry> peers;
    };
}
