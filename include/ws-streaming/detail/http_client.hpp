#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/detail/http_version.hpp>
#include <ws-streaming/detail/ssl_stream_traits.hpp>

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
     * This class is templated on the stream type. Two instantiations are provided below:
     *   - http_client - a plaintext HTTP client over boost::beast::tcp_stream.
     *   - https_client - a TLS HTTP client over boost::asio::ssl::stream<boost::beast::tcp_stream>.
     * A client object can perform multiple async_request() calls, but only one connection attempt
     * at a time may be in progress. Connection attempts can be canceled by calling cancel(); the
     * completion handler will then be called with the error code
     * boost::asio::error::operation_aborted.
     *
     * HTTP client instances must always be owned by a std::shared_ptr.
     */
    template <typename Stream>
    class basic_http_client
        : public std::enable_shared_from_this<basic_http_client<Stream>>
    {
        public:

            static constexpr bool is_tls = is_ssl_stream_v<Stream>;

            using response_type = boost::beast::http::response<boost::beast::http::string_body>;

            using handler_type = std::function<
                void(
                    const boost::system::error_code& ec,
                    const response_type& response,
                    Stream& stream,
                    const boost::beast::flat_buffer& buffer)>;

            /**
             * Constructs a plaintext HTTP client object. Only available for the non-TLS instantiation.
             * Asynchronous socket operations will be dispatched using the specified execution context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             */
            template <typename S = Stream,
                std::enable_if_t<!is_ssl_stream_v<S>, int> = 0>
            explicit basic_http_client(boost::asio::any_io_executor executor)
                : _resolver(executor)
                , _stream(executor)
            {
            }

#if WS_STREAMING_ENABLE_TLS

            /**
             * Constructs a TLS HTTP client object. Only available for the TLS instantiation.
             * Asynchronous socket operations will be dispatched using the specified execution context,
             * and TLS operations will use the specified SSL context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             * @param ssl_context The SSL context to use for TLS operations.
             */
            template <typename S = Stream,
                std::enable_if_t<is_ssl_stream_v<S>, int> = 0>
            basic_http_client(
                    boost::asio::any_io_executor executor,
                    boost::asio::ssl::context& ssl_context)
                : _resolver(executor)
                , _stream(executor, ssl_context)
            {
            }

#endif

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
             * @param port The TCP port number or well-known port name of the HTTP server.
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
                const std::string& port,
                boost::beast::http::request<boost::beast::http::string_body>&& request,
                handler_type handler)
            {
                _handler = std::move(handler);

                request.set(boost::beast::http::field::user_agent, http_product_string());

                request.prepare_payload();
                _request = std::move(request);

                _resolver.async_resolve(
                    hostname,
                    port,
                    [self_weak = this->weak_from_this()](const boost::system::error_code& ec,
                                const boost::asio::ip::tcp::resolver::results_type& results)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_resolve(ec, results);
                    });
            }

            /**
             * Cancels a pending request, if any. The handler passed to async_request() will be
             * called with the error code boost::asio::error::operation_aborted, if it has not
             * already been called.
             */
            void cancel()
            {
                _resolver.cancel();
                tcp_layer().cancel();
            }

        private:

            boost::beast::tcp_stream& tcp_layer()
            {
                if constexpr (is_tls)
                    return _stream.next_layer();
                else
                    return _stream;
            }

            void finish_resolve(
                const boost::system::error_code& ec,
                const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return complete(ec);

                tcp_layer().async_connect(
                    results,
                    [self_weak = this->weak_from_this()](const boost::system::error_code& ec, auto /*endpoint*/)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_connect(ec);
                    });
            }

            void finish_connect(
                const boost::system::error_code& ec)
            {
                if (ec)
                    return complete(ec);

                tcp_layer().expires_after(std::chrono::seconds(30));

                if constexpr (is_tls)
                {
                    _stream.async_handshake(
                        Stream::handshake_type::client,
                        [self_weak = this->weak_from_this()](const boost::system::error_code& ec)
                        {
                            if (auto self = self_weak.lock())
                                self->finish_handshake(ec);
                        });
                }
                else
                {
                    do_write();
                }
            }

            void finish_handshake(
                const boost::system::error_code& ec)
            {
                if (ec)
                    return complete(ec);

                do_write();
            }

            void do_write()
            {
                boost::beast::http::async_write(
                    _stream,
                    _request,
                    [self_weak = this->weak_from_this()](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_write(ec);
                    });
            }

            void finish_write(
                const boost::system::error_code& ec)
            {
                if (ec)
                    return complete(ec);

                _buffer.clear();

                boost::beast::http::async_read(
                    _stream,
                    _buffer,
                    _response,
                    [self_weak = this->weak_from_this()](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_read(ec);
                    });
            }

            void finish_read(
                const boost::system::error_code& ec)
            {
                if (ec)
                    return complete(ec);

                complete();
            }

            void complete(const boost::system::error_code& ec = {})
            {
                if (!_handler)
                    return;

                _handler(ec, _response, _stream, _buffer);
                _handler = {};
            }

        private:

            boost::asio::ip::tcp::resolver _resolver;
            Stream _stream;
            boost::beast::http::request<boost::beast::http::string_body> _request;
            boost::beast::flat_buffer _buffer;
            response_type _response;

            handler_type _handler;
    };

    using http_client = basic_http_client<boost::beast::tcp_stream>;

#if WS_STREAMING_ENABLE_TLS
    using https_client = basic_http_client<boost::asio::ssl::stream<boost::beast::tcp_stream>>;
#endif
}
