#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/optional/optional.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/http_version.hpp>
#include <ws-streaming/detail/ssl_stream_traits.hpp>
#include <ws-streaming/detail/websocket_protocol.hpp>

namespace wss::detail
{
    class http_servicer_base
    {
        public:
            virtual ~http_servicer_base() = default;
            virtual void run() = 0;
            virtual void stop() = 0;
    };

    /**
     * Implements an asynchronous HTTP server which accepts WebSocket Streaming Protocol command
     * interface requests and WebSocket connections. Servicer objects manage a single connection.
     * If an HTTP command interface request is received, the on_command_interface_request signal
     * is raised. Connected slots should handle the request and return a response to be
     * transmitted to the client. If a WebSocket upgrade request is received, the
     * on_websocket_upgrade signal is raised. When the connection is closed, or after a WebSocket
     * upgrade request has been handled, the on_closed event is raised.
     *
     * This class is templated on the stream type. Two instantiations are provided below:
     *   - http_client_servicer   - a plaintext servicer over boost::beast::tcp_stream.
     *   - https_client_servicer  - a TLS servicer over boost::asio::ssl::stream<boost::beast::tcp_stream>.
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
    template <typename Stream>
    class basic_http_client_servicer
        : public http_servicer_base
        , public std::enable_shared_from_this<basic_http_client_servicer<Stream>>
    {
        public:

            static constexpr bool is_tls = is_ssl_stream_v<Stream>;

            // On a WebSocket upgrade the plaintext servicer releases and hands out the bare tcp::socket
            // whereas the TLS servicer must hand out the whole ssl::stream
            // (releasing the socket would discard the live TLS session)
            using upgrade_socket_type = std::conditional_t<is_tls, Stream, boost::asio::ip::tcp::socket>;

            /**
             * Constructs a plaintext servicer object, taking ownership of the specified socket.
             * Only available for the non-TLS instantiation. No asynchronous operations are started
             * until run() is called.
             *
             * @param socket A socket, which the constructed object takes ownership of. The socket
             *     should be connected to the HTTP client.
             */
            template <typename S = Stream,
                std::enable_if_t<!is_ssl_stream_v<S>, int> = 0>
            explicit basic_http_client_servicer(boost::asio::ip::tcp::socket&& socket)
                : stream(std::move(socket))
            {
            }

#if WS_STREAMING_ENABLE_TLS

            /**
             * Constructs a TLS servicer object, taking ownership of the specified socket and
             * wrapping it in a TLS stream. Only available for the TLS instantiation. The TLS
             * handshake is performed when run() is called.
             *
             * @param socket A connected socket, which the constructed object takes ownership of.
             * @param ssl_context The SSL context to use for TLS operations. Must outlive the
             *     servicer.
             */
            template <typename S = Stream,
                std::enable_if_t<is_ssl_stream_v<S>, int> = 0>
            basic_http_client_servicer(
                    boost::asio::ip::tcp::socket&& socket,
                    boost::asio::ssl::context& ssl_context)
                : stream(std::move(socket), ssl_context)
            {
            }

#endif

            /**
             * Activates the servicer by starting asynchronous I/O operations using the socket's
             * execution context. For the TLS instantiation, a TLS handshake is performed first. To
             * stop the servicer later, call stop().
             */
            void run() override
            {
                if constexpr (is_tls)
                {
                    tcp_layer().expires_after(std::chrono::seconds(30));

                    stream.async_handshake(
                        Stream::handshake_type::server,
                        [self_weak = this->weak_from_this()](const boost::system::error_code& ec)
                        {
                            if (auto self = self_weak.lock())
                                self->finish_handshake(ec);
                        });
                }
                else
                {
                    do_read();
                }
            }

            /**
             * Closes the connection. The socket is closed, and any pending asynchronous socket
             * operations are canceled, but their completion handlers, which hold shared-pointer
             * references to this object, will be posted to the execution context and execute
             * later.
             */
            void stop() override
            {
                boost::asio::post(
                    stream.get_executor(),
                    [self_weak = this->weak_from_this()]()
                    {
                        if (auto self = self_weak.lock())
                            self->close();
                    });
            }

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

            /**
             * A signal raised when a WebSocket upgrade has been completed. For the plaintext
             * servicer the argument is the released bare tcp::socket; for the TLS servicer it is
             * the ssl::stream, carrying the live TLS session. A connected slot should take
             * ownership by moving from the argument.
             */
            boost::signals2::signal<
                void(upgrade_socket_type&)
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

            boost::beast::tcp_stream& tcp_layer()
            {
                if constexpr (is_tls)
                    return stream.next_layer();
                else
                    return stream;
            }

            void finish_handshake(
                const boost::system::error_code& ec)
            {
                if (ec)
                    return close(ec);

                do_read();
            }

            void do_read()
            {
                tcp_layer().expires_after(std::chrono::seconds(30));

                boost::beast::http::async_read(
                    stream,
                    buffer,
                    req = {},
                    [self_weak = this->weak_from_this()](const boost::system::error_code& ec, std::size_t bytes_transferred)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_read(ec, bytes_transferred);
                    });
            }

            void do_write(
                boost::beast::http::message_generator&& msg,
                response_actions action)
            {
                boost::beast::async_write(
                    stream,
                    std::move(msg),
                    [self_weak = this->weak_from_this(), action](const boost::system::error_code& ec, std::size_t bytes_transferred)
                    {
                        if (auto self = self_weak.lock())
                            self->finish_write(action, ec, bytes_transferred);
                    });
            }

            void finish_read(
                const boost::system::error_code& ec,
                std::size_t /*bytes_transferred*/)
            {
                if (ec)
                    return close(ec);

                auto get_header = [&](boost::beast::http::field field)
                {
                    auto it = std::find_if(
                        req.begin(),
                        req.end(),
                        [&](const auto& header)
                        {
                            return header.name() == field;
                        });

                    return it == req.end() ? "" : it->value();
                };

                auto key = get_header(boost::beast::http::field::sec_websocket_key);

                if (get_header(boost::beast::http::field::upgrade) == "websocket" && !key.empty())
                {
                    auto response_key = detail::websocket_protocol::get_response_key(key);

                    boost::beast::http::response<boost::beast::http::string_body> res(
                        boost::beast::http::status::switching_protocols,
                        req.version());

                    res.set(boost::beast::http::field::server, http_product_string());

                    res.set(boost::beast::http::field::connection, "Upgrade");
                    res.set(boost::beast::http::field::upgrade, "websocket");
                    res.set(boost::beast::http::field::sec_websocket_accept, response_key);

                    do_response(res);
                }
                else if (req.method() == boost::beast::http::verb::post)
                {
                    nlohmann::json request_json;

                    try
                    {
                        request_json = nlohmann::json::parse(req.body());
                    }

                    catch (const nlohmann::json::exception& ex)
                    {
                        return do_response(
                            req,
                            boost::beast::http::status::internal_server_error,
                            { { "code", -32700 }, { "message", ex.what() } });
                    }

                    if (!request_json.is_object()
                            || !request_json.contains("method")
                            || !request_json["method"].is_string())
                        return do_response(
                            req,
                            boost::beast::http::status::bad_request,
                            { { "code", -32700 }, { "message", "Request object is invalid" } });

                    boost::optional<nlohmann::json> response_json;

                    try
                    {
                        response_json = on_command_interface_request(
                            request_json["method"],
                            request_json.contains("params")
                                ? request_json["params"]
                                : nlohmann::json{nullptr});
                    }

                    catch (const std::exception& ex)
                    {
                        return do_response(
                            req,
                            boost::beast::http::status::internal_server_error,
                            { { "code", -32700 }, { "message", ex.what() } });
                    }

                    if (!response_json.has_value())
                        return do_response(
                            req,
                            boost::beast::http::status::internal_server_error,
                            { { "code", -32700 }, { "message", "No connected slot" } });


                    return do_response(
                        req,
                        boost::beast::http::status::ok,
                        response_json.value());
                }
                else if (req.method() == boost::beast::http::verb::options)
                {
                    return do_response(
                        req,
                        boost::beast::http::status::no_content,
                        nullptr);
                }
                else
                {
                    boost::beast::http::response<boost::beast::http::string_body> res(
                        boost::beast::http::status::bad_request,
                        req.version());

                    res.set(
                        boost::beast::http::field::server,
                        http_product_string());

                    res.keep_alive(req.keep_alive());
                    res.set(boost::beast::http::field::access_control_allow_headers, "*");
                    res.set(boost::beast::http::field::access_control_allow_origin, "*");
                    res.prepare_payload();

                    do_write(
                        std::move(res),
                        req.keep_alive()
                            ? response_actions::keepalive
                            : response_actions::close);
                }
            }

            void finish_write(
                response_actions action,
                const boost::beast::error_code& ec,
                std::size_t /*bytes_transferred*/)
            {
                if (ec)
                    return close(ec);

                switch (action)
                {
                    case response_actions::keepalive:
                        return do_read();

                    case response_actions::upgrade:
                    {
                        if constexpr (is_tls)
                        {
                            // TLS stream cannot be detached from its tcp_stream the way the
                            // plaintext branch below detaches the bare socket. The deadline
                            // armed for the HTTP phase would travel to the new owner and close
                            // the socket mid-session; disarm it explicitly instead.
                            tcp_layer().expires_never();
                            return on_websocket_upgrade(stream);
                        }
                        else
                        {
                            // release_socket() cancels the deadline
                            auto socket = stream.release_socket();
                            return on_websocket_upgrade(socket);
                        }
                    }

                    default:
                        return close();
                }
            }

            void do_response(
                const boost::beast::http::request<boost::beast::http::string_body>& req,
                boost::beast::http::status status,
                const nlohmann::json& response_json)
            {
                boost::beast::http::response<boost::beast::http::string_body> res(
                    status,
                    req.version());

                res.keep_alive(req.keep_alive());
                res.set(boost::beast::http::field::access_control_allow_headers, "*");
                res.set(boost::beast::http::field::access_control_allow_origin, "*");

                if (!response_json.is_null())
                {
                    bool is_success;

                    if (response_json.is_array())
                    {
                        is_success = true;
                        for (const auto& entry : response_json)
                        {
                            if (!entry.is_boolean() || entry != true)
                            {
                                is_success = false;
                                break;
                            }
                        }
                    }
                    else if (response_json.is_boolean())
                    {
                        is_success = response_json == true;
                    }
                    else
                    {
                        is_success = false;
                    }

                    if (is_success)
                    {
                        res.set(boost::beast::http::field::content_type, "text/plain");
                        res.body() = "Succeeded";
                    }
                    else
                    {
                        res.set(boost::beast::http::field::content_type, "application/json");
                        res.body() = response_json.dump();
                    }
                }

                do_response(res);
            }

            template <typename Body>
            void do_response(boost::beast::http::response<Body>& response)
            {
                response.set(
                    boost::beast::http::field::server,
                    http_product_string());

                response.prepare_payload();

                do_write(
                    std::move(response),
                    response.result() == boost::beast::http::status::switching_protocols
                        ? response_actions::upgrade
                        : response.keep_alive()
                            ? response_actions::keepalive
                            : response_actions::close);
            }

            void close(const boost::system::error_code& ec = {})
            {
                if (ec == boost::beast::error::timeout)
                    return on_closed(ec);

                if (!tcp_layer().socket().is_open())
                    return;

                boost::beast::error_code shutdown_ec;
                tcp_layer().socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, shutdown_ec);
                tcp_layer().close();

                on_closed(ec);
            }

        private:

            Stream stream;
            boost::beast::flat_buffer buffer;
            boost::beast::http::request<boost::beast::http::string_body> req;
    };

    using http_client_servicer = basic_http_client_servicer<boost::beast::tcp_stream>;

#if WS_STREAMING_ENABLE_TLS
    using https_client_servicer = basic_http_client_servicer<boost::asio::ssl::stream<boost::beast::tcp_stream>>;
#endif
}
