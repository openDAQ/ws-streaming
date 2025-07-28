#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url.hpp>

#include <ws-streaming/connection.hpp>

namespace wss
{
    /**
     * Asynchronously establishes a WebSocket Streaming connection by making an HTTP/WebSocket
     * request to a remote server. The caller calls connect() with a WebSocket URL. When the
     * WebSocket connection is established, the on_connected signal is raised with a constructed
     * wss::connection object. If an error occurs, the on_error signal is raised.
     */
    class client : public std::enable_shared_from_this<client>
    {
        public:

            /**
             * Constructs a client object. No asynchronous operations are started until connect()
             * is called. Asynchronous socket operations will be dispatched using the specified
             * execution context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             */
            client(boost::asio::any_io_executor executor);

            /**
             * Asynchronously connects to a remote server. An HTTP GET request is made to
             * establish the WebSocket connection. This function returns immediately. Either the
             * on_connected or on_error signals will be raised from the specified execution
             * context.
             *
             * @param url The WebSocket URL of the remote server.
             */
            void connect(const boost::urls::url_view& url);

            /**
             * A signal raised when a connection is successfully established.
             *
             * @param connection The constructed connection object. The connection::run() function
             *     is called before the signal is raised. Slots should therefore make any desired
             *     signal connections before the slot returns.
             */
            boost::signals2::signal<
                void(const std::shared_ptr<wss::connection>& connection)
            > on_connected;

            /**
             * A signal raised when a connection attempt fails.
             *
             * @param ec An error code describing the error.
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
            > on_error;

        private:

            void prepare_request(
                const std::string& path);

            void finish_resolve(
                const boost::system::error_code& ec,
                const boost::asio::ip::tcp::resolver::results_type& results);

            void finish_connect(
                const boost::system::error_code& ec);

            void finish_write(
                const boost::system::error_code& ec);

            void finish_read(
                const boost::system::error_code& ec);

            std::string get_random_key();

        private:

            std::string _hostname;
            boost::asio::ip::tcp::resolver _resolver;
            boost::beast::tcp_stream _stream;
            boost::beast::http::request<boost::beast::http::string_body> _request;
            boost::beast::flat_buffer _buffer;
            boost::beast::http::response<boost::beast::http::empty_body> _response;
    };
}
