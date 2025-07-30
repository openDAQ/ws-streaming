#pragma once

#include <cstddef>
#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

namespace wss::transport
{
    /**
     * Implements an asynchronous HTTP server which accepts WebSocket Streaming Protocol command
     * interface requests and WebSocket connections. Servicer objects manage a single connection.
     * If an HTTP command interface request is received, the on_command_interface_request signal
     * is raised. Connected slots should handle the request and return a response to be
     * transmitted to the client. If a WebSocket upgrade request is received, the
     * on_websocket_upgrade signal is raised. When the connection is closed, or after a WebSocket
     * upgrade request has been handled, the on_closed event is raised.
     *
     * Servicer objects are constructed with, and take ownership of, a connected Boost.Asio
     * socket, and must always be managed by a std::shared_ptr, following the normal Boost.Asio
     * pattern. When run() is called, the servicer object performs asynchronous I/O operations
     * using the execution context of the provided socket. This execution context must provide
     * sequential execution, i.e. in the terminology of Boost.Asio, it must be an explicit or
     * implicit strand. In addition, the caller must ensure no member functions are called
     * concurrently with each other or with an asynchronous completion handler. More explicitly
     * stated, this class is not thread-safe.
     */
    class http_client_servicer : public std::enable_shared_from_this<http_client_servicer>
    {
        public:

            /**
             * Constructs a servicer object, taking ownership of the specified socket. No
             * asynchronous operations are started until run() is called. Asynchronous socket
             * operations will be dispatched using the socket's execution context.
             *
             * @param socket A socket, which the constructed object takes ownership of. The socket
             *     should be connected to the HTTP client.
             */
            http_client_servicer(boost::asio::ip::tcp::socket&& socket);

            /**
             * Activates the servicer by starting asynchronous I/O operations using the socket's
             * execution context. To stop the servicer later, call stop(), which cancels all
             * asynchronous I/O operations, allowing the object to be destroyed.
             */
            void run();

            /**
             * Closes the connection. The socket is closed, and any pending asynchronous socket
             * operations are canceled, but their completion handlers, which hold shared-pointer
             * references to this object, will be posted to the execution context and execute
             * later. The on_closed signal will be raised later from the execution context,
             * unless it has already been raised due to an error or a previous call to stop().
             */
            void stop();

            /**
             * A signal raised when a client issues a JSON-RPC command interface request.
             * Connected slots should service the request and return a JSON response object to be
             * sent back to the client. If multiple slots are connected, the return value of the
             * last (most recently connected) slot is used. If no slots are connected, or if a
             * slot throws an exception, a 500 Internal Server Error response is sent back to the
             * client.
             *
             * @param method The command interface request method name.
             * @param params A JSON value containing the command interface request parameters.
             *
             * @throws std::exception An error occurred. A 500 Internal Server Error response is
             *     sent back to the client.
             * @throws ... Connected slots should not throw exceptions not derived from
             *     std::exception. If they do, they will propagate out to the execution context.
             *     This can result in an unhandled exception on a thread and terminate the
             *     process.
             */
            boost::signals2::signal<
                nlohmann::json(
                    const std::string& method,
                    const nlohmann::json& params)
            > on_command_interface_request;

            boost::signals2::signal<
                void(boost::asio::ip::tcp::socket&)
            > on_websocket_upgrade;

            /**
             * A signal raised when the connection is closed. This can occur due to an error, or
             * when stop() is called. The signal is raised from the execution context of the
             * socket. Note that this may occur even after a caller has released its
             * std::shared_ptr reference to a servicer object.
             *
             * @param ec The error code of the error, if any, that caused the connection to be
             *     closed.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
            > on_closed;

        private:

            enum class response_actions
            {
                close,
                keepalive,
                upgrade
            };

            void do_read();

            void do_write(
                boost::beast::http::message_generator&& msg,
                response_actions action);

            void finish_read(
                const boost::system::error_code& ec,
                std::size_t bytes_transferred);

            void finish_write(
                response_actions action,
                const boost::system::error_code& ec,
                std::size_t bytes_transferred);

            void do_response(
                const boost::beast::http::request<boost::beast::http::string_body>& req,
                boost::beast::http::status status,
                const nlohmann::json& response_json);

            template <typename Body>
            void do_response(boost::beast::http::response<Body>& response);

            void close(const boost::system::error_code& ec = {});

        private:

            boost::beast::tcp_stream stream;
            boost::beast::flat_buffer buffer;
            boost::beast::http::request<boost::beast::http::string_body> req;
    };
}
