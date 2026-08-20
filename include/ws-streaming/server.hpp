#pragma once

#include <atomic>
#include <list>
#include <memory>
#include <set>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/listener.hpp>
#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/connected_client.hpp>
#include <ws-streaming/detail/connected_client_iterator.hpp>
#include <ws-streaming/detail/http_client_servicer.hpp>

#if WS_STREAMING_ENABLE_TLS
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#endif

namespace wss
{
    /**
     * Asynchronously accepts and manages WebSocket Streaming connections from clients. The
     * application configures the server with one or more TCP listeners by calling add_listener(),
     * or by calling add_default_listeners() to use the default port numbers specified by the
     * WebSocket Streaming specification. Listeners accepting TLS-encrypted connections are added
     * with add_tls_listener(), and can be used alongside unencrypted ones. The application then
     * calls run() to begin listening for connections.
     *
     * A server can publish signal data to connected clients. The application should call
     * add_local_signal() for each signal to be published. Signals are advertised as available to
     * all connected clients.
     *
     * A server can also consume signal data from connected clients. The on_available event
     * aggregates the connection::on_available events from all connected clients, so that an
     * application can react to signal availability without being aware of or needing to manage
     * the individual connections.
     */
    class server
    {
        public:

            /**
             * An iterator type that iterates over the set of connected clients. Such iterators
             * are returned by the begin() and end() member functions.
             */
            typedef detail::connected_client_iterator iterator;

            /**
             * Constructs a server object. Asynchronous socket operations will be dispatched using
             * the specified execution context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             */
            server(boost::asio::any_io_executor executor);

            /**
             * Adds a listener so that the server listens on the specified TCP port number.
             *
             * @param port The port number to listen on.
             * @param make_command_interface True to set this port as the HTTP JSON-RPC
             *     command interface port, to which clients that do not support the in-band
             *     command interface can connect to submit command interface requests.
             */
            void add_listener(
                std::uint16_t port,
                bool make_command_interface = false);

            /**
             * Adds a listener.
             *
             * This function must be called before calling run().
             *
             * @param listener The listener object to use.
             */
            void add_listener(std::shared_ptr<listener<>> listener);

#if WS_STREAMING_ENABLE_TLS

            /**
             * Adds a TLS listener so that the server accepts TLS-encrypted WebSocket connections
             * on the specified TCP port number. The library builds and owns the SSL context
             * from the supplied certificate files.
             *
             * A server object holds a single SSL context, which is shared by all of its TLS
             * listeners. Calling this function more than once therefore replaces the certificate
             * files used by any TLS listener already added.
             *
             * Clients connecting with a `wss://` URL that does not specify a port number use port
             * 7415.
             *
             * This function must be called before calling run(). It is only available in a
             * build with TLS support.
             *
             * @param port The port number to listen on.
             * @param cert_file Path to the server certificate chain (PEM). Required.
             * @param key_file Path to the server private key (PEM). Required.
             * @param ca_file Path to a PEM file of trusted CA certificates used to verify client
             *     certificates. If non-empty, mutual TLS is enabled: clients must present a
             *     certificate signed by one of these CAs. If empty, client certificates are not
             *     requested.
             * @param make_command_interface True to set this port as the HTTP JSON-RPC command
             *     interface port. A server advertises a single command interface port to every
             *     peer, whatever transport that peer is using, and the advertisement does not
             *     indicate whether TLS is required. The library's own out-of-band command
             *     interface client always connects in plaintext, and so cannot use a command
             *     interface hosted on a TLS port; peers implementing version 3.0 or later of the
             *     protocol use the in-band command interface instead.
             *
             * @throws boost::system::system_error A certificate or key file could not be loaded.
             */
            void add_tls_listener(
                std::uint16_t port,
                const std::string& cert_file,
                const std::string& key_file,
                const std::string& ca_file = {},
                bool make_command_interface = false);

#endif

            /**
             * Adds listeners for the standard port numbers specified by the WebSocket Streaming
             * Specification, namely 7414 and 7438. This function is equivalent to calling
             * add_listener(std::uint16_t) for these two ports.
             *
             *  This function must be called before calling run().
             */
            void add_default_listeners();

            /**
             * Activates the server object by scheduling asynchronous I/O operations with the
             * execution context passed to the constructor. Do not call add_listener() or
             * add_default_listeners() after activating the server.
             */
            void run();

            /**
             * Registers a local signal with the server. The signal will be advertised as
             * available to all current and future clients. The connection object(s) connect to
             * the signal's Boost.Signals2 signals so that data published to the signal can be
             * transmitted to remote peers, if subscribed.
             *
             * @param signal The local signal to register. The server object holds a reference
             *     to this object, and it should not be destroyed until remove_local_signal() has
             *     returned or the on_closed signal has been raised.
             */
            void add_local_signal(local_signal& signal);

            /**
             * Unregisters a local signal from the server. The signal will be advertised as
             * unavailable to any connected clients. The server object disconnects from the
             * signal's Boost.Signals2 signals.
             *
             * @param signal The local signal to unregister.
             */
            void remove_local_signal(local_signal& signal);

            /**
             * Shuts down the server by closing all active connections and stopping all listeners.
             * The on_unavailable event will be raised for each signal currently available from an
             * active connection. Then the on_client_disconnected event will be raised for each
             * connection.
             */
            void close();

            /**
             * Tests if close() has been called on this server. This function is thread-safe, but
             * there is a potential race condition of this function is not called from the
             * execution context used to construct the server.
             *
             * @return True if close() has been called on this server.
             */
            bool closed() { return _closed; }

            /**
             * Gets the execution context being used for asynchronous I/O operations.
             *
             * @param The execution context being used for asynchronous I/O operations.
             */
            boost::asio::any_io_executor& executor() { return _executor; }

            /**
             * Gets an iterator to the first connected client.
             *
             * @return An iterator to the first connected client.
             */
            iterator begin() noexcept { return iterator{_clients.begin()}; }

            /**
             * Gets an iterator past the last connected client.
             *
             * @return An iterator past the last connected client.
             */
            iterator end() noexcept { return iterator{_clients.end()}; }

            /**
             * A Boost.Signals2 signal raised when a new connection has been established to the
             * server.
             *
             * @param connection The connection object.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(connection_ptr& connection)
            > on_client_connected;

            /**
             * A Boost.Signals2 signal raised when a new remote signal becomes known to the
             * server from a client connection after being advertised by the remote peer.
             *
             * @param connection The connection which is making the signal available.
             * @param signal A std::shared_ptr to the newly available signal.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(
                    connection_ptr connection,
                    remote_signal_ptr signal)
            > on_available;

            /**
             * A Boost.Signals2 signal raised when a remote signal is no longer available from a
             * client connection. This can occur if the remote peer indicates the signal is no
             * longer available, or when the connection has been closed. No further event signals
             * will be raised by the remote_signal object.
             *
             * @param connection The connection which made the signal available.
             * @param signal A std::shared_ptr to the signal that is no longer available.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(
                    connection_ptr connection,
                    remote_signal_ptr signal)
            > on_unavailable;

            /**
             * A Boost.Signals2 signal raised when a connection has been closed.
             *
             * @param connection The connection object.
             * @param ec An error code describing the reason for the closure.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(
                    connection_ptr& connection,
                    const boost::system::error_code& ec)
            > on_client_disconnected;

            /**
             * A Boost.Signals2 signal raised when the server has been shut down.
             *
             * @param ec An error code describing the reason for the shutdown.
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
            > on_closed;

        private:

            // Accepts a plaintext connection and starts a plaintext servicer session
            void on_listener_accept(
                boost::asio::ip::tcp::socket& socket);

#if WS_STREAMING_ENABLE_TLS

            // Accepts a connection on a TLS listener and starts a TLS servicer session
            // The TLS handshake is performed by the servicer
            void on_tls_listener_accept(
                boost::asio::ip::tcp::socket& socket);

#endif

            // Wires up a newly constructed servicer (plaintext or TLS) into a session.
            // Templated on the servicer type so that the correct WebSocket-upgrade
            // slot (bare socket vs ssl::stream) is connected.
            template <typename Servicer>
            void start_session(std::shared_ptr<Servicer> client)
            {
                _sessions.emplace_back(
                    client,
                    client->on_command_interface_request.connect(
                        std::bind(&server::on_servicer_command_interface_request, this, std::placeholders::_1, std::placeholders::_2)),
                    client->on_websocket_upgrade.connect(
                        std::bind(&server::on_servicer_websocket_upgrade<typename Servicer::upgrade_socket_type>, this, std::placeholders::_1)),
                    client->on_closed.connect(
                        std::bind(&server::on_servicer_closed, this,
                            std::weak_ptr<detail::http_servicer_base>(client), std::placeholders::_1)));

                client->run();
            }

            nlohmann::json on_servicer_command_interface_request(
                const std::string& method,
                const nlohmann::json& params);

            // Returns the underlying tcp::socket for either a bare socket or a TLS stream
            static boost::asio::ip::tcp::socket& tcp_socket_of(boost::asio::ip::tcp::socket& socket)
            {
                return socket;
            }

#if WS_STREAMING_ENABLE_TLS

            static boost::asio::ip::tcp::socket& tcp_socket_of(boost::asio::ssl::stream<boost::beast::tcp_stream>& stream)
            {
                return stream.next_layer().socket();
            }

#endif

            // Handles a completed WebSocket upgrade by taking ownership
            // of the transport (bare socket or ssl::stream) and building a connection from it
            template <typename UpgradeSocket>
            void on_servicer_websocket_upgrade(UpgradeSocket& upgrade_socket)
            {
                std::string connection_local_stream_id;
                try
                {
                    auto remote_endpoint = tcp_socket_of(upgrade_socket).remote_endpoint();
                    connection_local_stream_id = remote_endpoint.address().to_string()
                                                 + ":" + std::to_string(remote_endpoint.port());
                }
                catch (const std::exception& /*e*/)
                {
                    return;
                }

                auto connection = std::make_shared<wss::connection>(
                    std::move(upgrade_socket),
                    false,
                    connection_local_stream_id);

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
                    std::bind(&server::on_connection_available, this, connection, std::placeholders::_1));

                entry.on_unavailable = connection->on_unavailable.connect(
                    std::bind(&server::on_connection_unavailable, this, connection, std::placeholders::_1));

                entry.on_disconnected = connection->on_disconnected.connect(
                    std::bind(&server::on_connection_disconnected, this, connection, std::placeholders::_1));

                connection->run();

                on_client_connected(connection);
            }

            void on_servicer_closed(
                std::weak_ptr<detail::http_servicer_base> servicer,
                const boost::system::error_code& ec);

            void on_connection_available(
                connection_ptr connection,
                remote_signal_ptr signal);

            void on_connection_unavailable(
                connection_ptr connection,
                remote_signal_ptr signal);

            void on_connection_disconnected(
                connection_ptr connection,
                const boost::system::error_code& ec);

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
                        std::shared_ptr<detail::http_servicer_base> client,
                        boost::signals2::scoped_connection on_command_interface_request,
                        boost::signals2::scoped_connection on_websocket_upgrade,
                        boost::signals2::scoped_connection on_closed)
                    : client(std::move(client))
                    , on_command_interface_request(std::move(on_command_interface_request))
                    , on_websocket_upgrade(std::move(on_websocket_upgrade))
                    , on_closed(std::move(on_closed))
                {
                }

                std::shared_ptr<detail::http_servicer_base> client;
                boost::signals2::scoped_connection on_command_interface_request;
                boost::signals2::scoped_connection on_websocket_upgrade;
                boost::signals2::scoped_connection on_closed;
            };

            std::atomic<bool> _closed = false;
            boost::asio::any_io_executor _executor;
            std::list<listener_entry> _listeners;
            std::list<client_entry> _sessions;
            std::list<detail::connected_client> _clients;
            std::set<local_signal *> _signals;
            std::list<local_signal *> _ordered_signals;
            std::uint16_t _command_interface_port = 0;
#if WS_STREAMING_ENABLE_TLS
            std::unique_ptr<boost::asio::ssl::context> _ssl_context;    // null if no TLS listener has been added
#endif
    };
}
