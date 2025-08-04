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
#include <ws-streaming/metadata.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/detail/local_signal_container.hpp>
#include <ws-streaming/detail/peer.hpp>
#include <ws-streaming/detail/remote_signal_container.hpp>
#include <ws-streaming/detail/remote_signal_impl.hpp>
#include <ws-streaming/detail/semver.hpp>

namespace wss
{
    /**
     * Implements a WebSocket Streaming connection to a remote peer.
     */
    class connection
        : public std::enable_shared_from_this<connection>
        , detail::local_signal_container
        , detail::remote_signal_container
    {
        public:

            /**
             * Constructs a connection object from a TCP socket. The socket must already have been
             * connected to the remote peer, including the HTTP WebSocket upgrade request and
             * response, if necessary.
             *
             * Do not directly construct a connection object. Connection instances must always be
             * managed by a std::shared_ptr; therefore, std::make_shared() should be used.
             *
             * After construction, the run() function must be called to register asynchronous
             * operations with the Boost.Asio execution context.
             *
             * @param socket A connected TCP socket. The constructed object takes ownership of the
             *     socket. The socket's execution context is used for all asynchronous I/O
             *     operations.
             * @param is_client True if the connection should behave as a client. The WebSocket
             *     Streaming Protocol is almost totally symmetric, except for a subtle difference
             *     during the handshake to support backward compatibility. A server transmits
             *     greeting information immediately. A client only transmits greeting information
             *     after the server has indicated a version number high enough to support
             *     symmetric connections.
             */
            connection(
                boost::asio::ip::tcp::socket&& socket,
                bool is_client);

            /**
             * Destroys a connection object.
             */
            ~connection();

            /**
             * Activates the connection object by scheduling asynchronous I/O operations with the
             * socket's execution context.
             */
            void run();

            /**
             * Activates the connection object by scheduling asynchronous I/O operations with the
             * socket's execution context. The specified data is consumed as if it had arrived
             * from the socket. This is useful in scenarios where previous code, such as for
             * performing an HTTP WebSocket upgrade request, inadvertently buffered WebSocket
             * Streaming Protocol data.
             *
             * @param data A pointer to the inadvertently buffered data.
             * @param size The number of bytes pointed to by @p data.
             */
            void run(const void *data, std::size_t size);

            /**
             * Deactivates the connection, disconnecting the underlying socket and canceling any
             * pending asynchronous I/O operations.
             *
             * All remote signals known to the connection are detached, causing their
             * remote_signal::on_unavailable signals to be raised. The connection::on_unavailable
             * signal is then raised for each signal. Finally, the on_disconnected signal is
             * raised with the error code boost::asio::error::operation_aborted.
             */
            void close();

            /**
             * Registers a local signal with the connection. The signal will be advertised as
             * available to the remote peer. The connection object connects to the signal's
             * Boost.Signals2 signals so that data published to the signal can be transmitted to
             * the remote peer, if subscribed.
             *
             * @param signal The local signal to register. The connection object holds a reference
             *     to this object, and it should not be destroyed until remove_local_signal() has
             *     returned or the on_disconnected signal has been raised.
             */
            void add_local_signal(local_signal& signal);

            /**
             * Unregisters a local signal from the connection. The signal will be advertised as
             * unavailable to the remote peer. The connection object disconnects from the signal's
             * Boost.Signals2 signals.
             *
             * @param signal The local signal to unregister.
             */
            void remove_local_signal(local_signal& signal);

            /**
             * Searches for a remote signal with the specified global identifier.
             *
             * @param id The global identifier of the remote signal to search for.
             *
             * @return A std::shared_ptr to the remote signal, if one with the specified @p id is
             *     known to the connection. Otherwise, an empty std::shared_ptr.
             */
            remote_signal_ptr
            find_remote_signal(const std::string& id) const;

            /**
             * Gets the Boost.Asio executor being used by the connection for asynchronous I/O
             * operations. This is the executor of the socket that was used to construct the
             * connection.
             *
             * @return The Boost.Asio executor being used by the connection for asynchronous I/O
             *     operations.
             */
            const boost::asio::any_io_executor& executor() const noexcept;

            /**
             * Gets the Boost.Asio socket underlying the connection.
             *
             * @return The Boost.Asio socket underlying the connection.
             */
            const boost::asio::ip::tcp::socket& socket() const noexcept;

            /**
             * A Boost.Signals2 signal raised when a new remote signal becomes known to the
             * connection after being advertised by the remote peer.
             *
             * @param signal A std::shared_ptr to the newly available signal.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(const remote_signal_ptr& signal)
            > on_available;

            /**
             * A Boost.Signals2 signal raised when a remote signal is no longer available from the
             * remote peer. This can occur if the remote peer indicates the signal is no longer
             * available, or when the connection has been closed. No further event signals will
             * be raised by the remote_signal object.
             *
             * @param signal A std::shared_ptr to the signal that is no longer available.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(const remote_signal_ptr& signal)
            > on_unavailable;

            /**
             * A Boost.Signals2 signal raised exactly once when the connection has been closed,
             * whether caused by an error or by calling close().
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
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
                detail::local_signal_container::local_signal_entry& signal);

            void on_local_signal_data_published(
                detail::local_signal_container::local_signal_entry& signal,
                std::int64_t domain_value,
                std::size_t sample_count,
                const void *data,
                std::size_t size);

            void on_signal_subscribe_requested(const std::string& signal_id);
            void on_signal_unsubscribe_requested(const std::string& signal_id);
            std::shared_ptr<detail::remote_signal_impl> on_signal_sought(const std::string& signal_id);

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

            bool subscribe(const std::string& signal_id, bool is_explicit);
            bool unsubscribe(const std::string& signal_id, bool is_explicit);

        private:

            bool _is_client;
            std::shared_ptr<detail::peer> _peer;

            detail::semver _api_version;
            std::string _remote_stream_id;
            std::string _local_stream_id;
            std::unique_ptr<detail::command_interface_client> _command_interface_client;

            boost::signals2::scoped_connection _on_peer_data_received;
            boost::signals2::scoped_connection _on_peer_metadata_received;
            boost::signals2::scoped_connection _on_peer_closed;

            bool _hello_sent = false;
    };

    /**
     * A convenience typedef for a std::shared_ptr holding a wss::connection object.
     */
    typedef std::shared_ptr<connection> connection_ptr;
}
