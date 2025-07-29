#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/error_code.hpp>

namespace wss::detail
{
    /**
     * An asynchronous HTTP client. The caller calls async_request() with a hostname / IP address,
     * port number, and populated HTTP request object. When the request is complete and a response
     * has been received, or when an error occurs, the completion handler passed to
     * async_request() is called with the corresponding error code and/or response object. The
     * completion handler also receives references to the underlying stream object and its buffer,
     * in case it wishes to take ownership of the connection (for example, for a WebSocket
     * upgrade).
     *
     * A client object can perform multiple async_request() calls, but only one connection attempt
     * at a time may be in progress. Connection attempts can be canceled by calling cancel(); the
     * completion handler will then be called with the error code
     * boost::asio::error::operation_aborted.
     *
     * HTTP client instances must always be owned by a std::shared_ptr.
     */
    class http_client : public std::enable_shared_from_this<http_client>
    {
        public:

            /**
             * Constructs an HTTP client object. Asynchronous socket operations will be dispatched
             * using the specified execution context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             */
            http_client(boost::asio::any_io_executor executor);

            /**
             * Asynchronously performs a request. A client object can perform multiple
             * async_request() calls, but only one connection attempt at a time may be in
             * progress. Connection attempts can be canceled by calling cancel(); the completion
             * handler will then be called with the error code
             * boost::asio::error::operation_aborted.
             *
             * Each request opens a new socket; keepalive is not supported.
             *
             * @param hostname The hostname or IP address of the HTTP server.
             * @param port The TCP port number of the HTTP server.
             * @param request A populated HTTP request object. The request object should not be
             *     "prepared"; i.e., do not call prepare_payload(). This function will add
             *     additional headers, such as `User-Agent`, to the request.
             * @param handler A completion handler to call when the operation is complete. This
             *     handler receives either a nonzero error code, or references to the response
             *     object as well as the underlying stream and buffer. These latter allow the
             *     handler, if it wishes, to take ownership of the connection; for example, for a
             *     WebSocket upgrade.
             */
            void async_request(
                const std::string& hostname,
                std::uint16_t port,
                boost::beast::http::request<boost::beast::http::string_body>&& request,
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const boost::beast::http::response<boost::beast::http::string_body>& response,
                        boost::beast::tcp_stream& stream,
                        const boost::beast::flat_buffer& buffer)
                > handler);

            /**
             * Cancels a pending request, if any. The handler passed to async_request() will be
             * called with the error code boost::asio::error::operation_aborted, if it has not
             * already been called. Note that it is possible a successful request has already been
             * scheduled with the execution context, resulting in the handler being called with
             * successful arguments even after calling this function. Cancellation guarantees only
             * that some call to the completion handler, successful or otherwise, will be
             * scheduled with the execution context.
             */
            void cancel();

        private:

            void finish_resolve(
                const boost::system::error_code& ec,
                const boost::asio::ip::tcp::resolver::results_type& results);

            void finish_connect(
                const boost::system::error_code& ec);

            void finish_write(
                const boost::system::error_code& ec);

            void finish_read(
                const boost::system::error_code& ec);

            void complete(const boost::system::error_code& ec = {});

        private:

            boost::asio::ip::tcp::resolver _resolver;
            boost::beast::tcp_stream _stream;
            boost::beast::http::request<boost::beast::http::string_body> _request;
            boost::beast::flat_buffer _buffer;
            boost::beast::http::response<boost::beast::http::string_body> _response;

            std::function<
                void(
                    const boost::system::error_code& ec,
                    const boost::beast::http::response<boost::beast::http::string_body>& response,
                    boost::beast::tcp_stream& stream,
                    const boost::beast::flat_buffer& buffer)
            > _handler;
    };
}
