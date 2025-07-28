#pragma once

#include <list>
#include <memory>

#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/transport/http_client_servicer.hpp>
#include <ws-streaming/transport/listener.hpp>
#include <ws-streaming/transport/peer.hpp>

namespace wss::transport
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
            void add_listener(std::shared_ptr<listener<>> listener);

            void on_listener_accept(boost::asio::ip::tcp::socket& socket);
            nlohmann::json on_servicer_control_request(const std::shared_ptr<http_client_servicer>& servicer, const nlohmann::json& request);
            void on_servicer_websocket_upgrade(const std::shared_ptr<http_client_servicer>& servicer, boost::asio::ip::tcp::socket& socket);
            void on_servicer_closed(const std::shared_ptr<http_client_servicer>& servicer, const boost::system::error_code& ec);
            void on_peer_data_received(const std::shared_ptr<peer>& peer, unsigned signo, const std::uint8_t *data, std::size_t size);
            void on_peer_metadata_received(const std::shared_ptr<peer>& peer, unsigned signo, const std::string& method, const nlohmann::json& params);
            void on_peer_closed(const std::shared_ptr<peer>& peer, const boost::system::error_code& ec);

            struct listener_entry
            {
                listener_entry(
                        std::shared_ptr<listener<>> l,
                        boost::signals2::scoped_connection connection)
                    : l(l)
                    , connection(std::move(connection))
                {
                }

                std::shared_ptr<listener<>> l;
                boost::signals2::scoped_connection connection;
            };

            struct client_entry
            {
                client_entry(
                        std::shared_ptr<http_client_servicer> client,
                        boost::signals2::scoped_connection on_websocket_upgrade,
                        boost::signals2::scoped_connection on_control_request,
                        boost::signals2::scoped_connection on_closed)
                    : client(client)
                    , on_websocket_upgrade(std::move(on_websocket_upgrade))
                    , on_control_request(std::move(on_control_request))
                    , on_closed(std::move(on_closed))
                {
                }

                std::shared_ptr<http_client_servicer> client;
                boost::signals2::scoped_connection on_websocket_upgrade;
                boost::signals2::scoped_connection on_control_request;
                boost::signals2::scoped_connection on_closed;
            };

            struct connected_client
            {
                connected_client(
                        std::shared_ptr<transport::peer> peer,
                        boost::signals2::scoped_connection on_data_received,
                        boost::signals2::scoped_connection on_metadata_received,
                        boost::signals2::scoped_connection on_closed)
                    : peer(peer)
                    , on_data_received(std::move(on_data_received))
                    , on_metadata_received(std::move(on_metadata_received))
                    , on_closed(std::move(on_closed))
                {
                }

                std::shared_ptr<transport::peer> peer;
                boost::signals2::scoped_connection on_data_received;
                boost::signals2::scoped_connection on_metadata_received;
                boost::signals2::scoped_connection on_closed;
            };

            boost::asio::strand<boost::asio::any_io_executor> strand;
            std::list<listener_entry> listeners;
            std::list<client_entry> sessions;
            std::list<connected_client> clients;
    };
}
