#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/transport/peer.hpp>
#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/detail/local_signal_container.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/semver.hpp>

namespace wss
{
    class connection
        : public std::enable_shared_from_this<connection>
        , detail::local_signal_container
    {
        public:

            connection(
                const std::string& hostname,
                boost::asio::ip::tcp::socket&& socket,
                bool is_client);

            void run();

            void run(const void *data, std::size_t size);

            void stop();

            void add_signal(local_signal& signal);

            void remove_signal(local_signal& signal);

            boost::signals2::signal<
                void(const std::shared_ptr<remote_signal>&)
            > on_available;

            boost::signals2::signal<
                void(const std::shared_ptr<remote_signal>&)
            > on_unavailable;

            boost::signals2::signal<
                void()
            > on_disconnected;

        private:

            void do_hello();

            void on_peer_data_received(
                unsigned signo,
                const std::uint8_t *data,
                std::size_t size);

            void on_peer_metadata_received(
                unsigned signo,
                const std::string& method,
                const nlohmann::json& params);

            void on_peer_closed(
                const boost::system::error_code& ec);

            void on_local_signal_metadata_changed(
                unsigned signo,
                const nlohmann::json& metadata);

            void on_local_signal_data(
                unsigned signo,
                const void *data,
                std::size_t size);

            void on_signal_subscribe_requested(const std::string& signal_id);
            void on_signal_unsubscribe_requested(const std::string& signal_id);

            void dispatch_metadata(
                unsigned signo,
                const std::string& method,
                const nlohmann::json& params);

            void handle_api_version(const nlohmann::json& params);
            void handle_init(const nlohmann::json& params);
            void handle_available(const nlohmann::json& params);
            void handle_subscribe(unsigned signo, const nlohmann::json& params);
            void handle_unsubscribe(unsigned signo, const nlohmann::json& params);
            void handle_unavailable(const nlohmann::json& params);
            void handle_command_interface_request(const nlohmann::json& params);
            void handle_command_interface_response(const nlohmann::json& params);

            nlohmann::json do_command_interface(const std::string& method, const nlohmann::json& params);
            nlohmann::json do_command_interface_subscribe(const nlohmann::json& params);
            nlohmann::json do_command_interface_unsubscribe(const nlohmann::json& params);

        private:

            struct signal_entry
            {
                signal_entry(const std::string& id)
                    : signal(std::make_shared<detail::remote_signal_impl>(id))
                {
                }

                std::shared_ptr<detail::remote_signal_impl> signal;
                boost::signals2::scoped_connection on_subscribe_requested;
                boost::signals2::scoped_connection on_unsubscribe_requested;
            };

            struct local_signal_entry
            {
                local_signal_entry(local_signal& signal)
                    : signal(signal)
                {
                }

                local_signal& signal;
                bool is_subscribed = false;
                boost::signals2::scoped_connection on_metadata_changed;
                boost::signals2::scoped_connection on_data;
            };

            std::string _hostname;
            bool _is_client;
            std::shared_ptr<transport::peer> _peer;
            std::map<std::string, signal_entry> _remote_signals_by_id;
            std::map<unsigned, signal_entry *> _remote_signals_by_signo;

            detail::semver _api_version;
            std::string _remote_stream_id;
            std::string _local_stream_id;
            std::unique_ptr<detail::command_interface_client> _command_interface_client;

            boost::signals2::scoped_connection _on_peer_data_received;
            boost::signals2::scoped_connection _on_peer_metadata_received;
            boost::signals2::scoped_connection _on_peer_closed;
    };
}
