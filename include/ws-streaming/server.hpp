#pragma once

#include <atomic>
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
#include <ws-streaming/detail/connected_client.hpp>
#include <ws-streaming/detail/connected_client_iterator.hpp>
#include <ws-streaming/detail/http_client_servicer.hpp>

namespace wss
{
    /**
     * Asynchronously accepts and manages WebSocket Streaming connections from clients. The
     * application configures the server with one or more TCP listeners by calling add_listener(),
     * or by calling add_default_listeners() to use the default port numbers specified by the
     * WebSocket Streaming specification. It then calls run() to begin listening for connections.
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

            void on_listener_accept(
                boost::asio::ip::tcp::socket& socket);

            nlohmann::json on_servicer_command_interface_request(
                const std::shared_ptr<detail::http_client_servicer>& servicer,
                const std::string& method,
                const nlohmann::json& params);

            void on_servicer_websocket_upgrade(const std::shared_ptr<detail::http_client_servicer>& servicer, boost::asio::ip::tcp::socket& socket);
            void on_servicer_closed(const std::shared_ptr<detail::http_client_servicer>& servicer, const boost::system::error_code& ec);

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

            std::atomic<bool> _closed = false;
            boost::asio::any_io_executor _executor;
            std::list<listener_entry> _listeners;
            std::list<client_entry> _sessions;
            std::list<detail::connected_client> _clients;
            std::set<local_signal *> _signals;
            std::list<local_signal *> _ordered_signals;
            std::uint16_t _command_interface_port = 0;
    };
}
