#pragma once

#include <list>
#include <memory>
#include <set>

#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/listener.hpp>
#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/http_client_servicer.hpp>

namespace wss
{
    class server
    {
        public:

            server(boost::asio::any_io_executor executor);

            void run();
            void stop();

            void add_signal(local_signal& signal);
            void remove_signal(local_signal& signal);

            boost::signals2::signal<
                void(
                    const std::shared_ptr<wss::connection>& connection,
                    const std::shared_ptr<wss::remote_signal>& signal)
            > on_available;

            boost::signals2::signal<
                void(
                    const std::shared_ptr<wss::connection>& connection)
            > on_client_connected;

            boost::signals2::signal<
                void(
                    const std::shared_ptr<wss::connection>& connection,
                    const std::shared_ptr<wss::remote_signal>& signal)
            > on_unavailable;

            boost::signals2::signal<
                void(
                    const std::shared_ptr<wss::connection>& connection)
            > on_client_disconnected;

        private:

            void add_listener(std::uint16_t port);
            void add_listener(std::shared_ptr<listener<>> listener);

            void on_listener_accept(boost::asio::ip::tcp::socket& socket);
            nlohmann::json on_servicer_command_interface_request(const std::shared_ptr<detail::http_client_servicer>& servicer, const nlohmann::json& request);

            void on_servicer_websocket_upgrade(const std::shared_ptr<detail::http_client_servicer>& servicer, boost::asio::ip::tcp::socket& socket);
            void on_servicer_closed(const std::shared_ptr<detail::http_client_servicer>& servicer, const boost::system::error_code& ec);

            void on_connection_available(
                const std::shared_ptr<wss::connection>& connection,
                const std::shared_ptr<remote_signal>& signal);

            void on_connection_unavailable(
                const std::shared_ptr<wss::connection>& connection,
                const std::shared_ptr<remote_signal>& signal);

            void on_connection_disconnected(
                const std::shared_ptr<wss::connection>& connection);

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
                        std::shared_ptr<detail::http_client_servicer> client,
                        boost::signals2::scoped_connection on_websocket_upgrade,
                        boost::signals2::scoped_connection on_command_interface_request,
                        boost::signals2::scoped_connection on_closed)
                    : client(client)
                    , on_websocket_upgrade(std::move(on_websocket_upgrade))
                    , on_command_interface_request(std::move(on_command_interface_request))
                    , on_closed(std::move(on_closed))
                {
                }

                std::shared_ptr<detail::http_client_servicer> client;
                boost::signals2::scoped_connection on_websocket_upgrade;
                boost::signals2::scoped_connection on_command_interface_request;
                boost::signals2::scoped_connection on_closed;
            };

            struct connected_client
            {
                connected_client(std::shared_ptr<wss::connection> connection)
                    : connection(connection)
                {
                }

                std::shared_ptr<wss::connection> connection;
                boost::signals2::scoped_connection on_available;
                boost::signals2::scoped_connection on_unavailable;
                boost::signals2::scoped_connection on_disconnected;
            };

            boost::asio::any_io_executor _executor;
            std::list<listener_entry> _listeners;
            std::list<client_entry> _sessions;
            std::list<connected_client> _clients;
            std::set<local_signal *> _signals;
    };
}
