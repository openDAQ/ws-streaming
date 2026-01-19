#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/async_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/error_code.hpp>

#include <ws-streaming/detail/http_client_servicer.hpp>
#include <ws-streaming/detail/websocket_protocol.hpp>

using namespace std::chrono_literals;
using namespace std::placeholders;

wss::detail::http_client_servicer::http_client_servicer(
        boost::asio::ip::tcp::socket&& socket)
    : stream(std::move(socket))
{
}

void wss::detail::http_client_servicer::run()
{
    do_read();
}

void wss::detail::http_client_servicer::stop()
{
    boost::asio::post(
        stream.get_executor(),
        [self = shared_from_this()]()
        {
            self->close();
        });
}

void wss::detail::http_client_servicer::do_read()
{
    stream.expires_after(30s);

    boost::beast::http::async_read(
        stream,
        buffer,
        req = {},
        std::bind(
            &http_client_servicer::finish_read,
            shared_from_this(),
            _1,
            _2));
}

void wss::detail::http_client_servicer::do_write(
    boost::beast::http::message_generator&& msg,
    response_actions action)
{
    boost::beast::async_write(
        stream,
        std::move(msg),
        std::bind(
            &http_client_servicer::finish_write,
            shared_from_this(),
            action,
            _1,
            _2));
}

void wss::detail::http_client_servicer::finish_read(
    const boost::system::error_code& ec,
    std::size_t bytes_transferred)
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

        res.set(
            boost::beast::http::field::server,
            "ws-streaming/" WS_STREAMING_VERSION_MAJOR
                "." WS_STREAMING_VERSION_MINOR
                "." WS_STREAMING_VERSION_PATCH
                " " BOOST_BEAST_VERSION_STRING);

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
            "ws-streaming/" WS_STREAMING_VERSION_MAJOR
                "." WS_STREAMING_VERSION_MINOR
                "." WS_STREAMING_VERSION_PATCH
                " " BOOST_BEAST_VERSION_STRING);

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

void wss::detail::http_client_servicer::finish_write(
    response_actions action,
    const boost::beast::error_code& ec,
    std::size_t bytes_transferred)
{
    if (ec)
        return close(ec);

    switch (action)
    {
        case response_actions::keepalive:
            return do_read();

        case response_actions::upgrade:
        {
            auto socket = stream.release_socket();
            return on_websocket_upgrade(socket);
        }

        default:
            return close();
    }
}

void wss::detail::http_client_servicer::do_response(
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
        res.set(boost::beast::http::field::content_type, "application/json");
        res.body() = response_json.dump();
    }

    do_response(res);
}

template <typename Body>
void wss::detail::http_client_servicer::do_response(
    boost::beast::http::response<Body>& response)
{
    response.set(
        boost::beast::http::field::server,
        "ws-streaming/" WS_STREAMING_VERSION_MAJOR
            "." WS_STREAMING_VERSION_MINOR
            "." WS_STREAMING_VERSION_PATCH
            " " BOOST_BEAST_VERSION_STRING);

    response.prepare_payload();

    do_write(
        std::move(response),
        response.result() == boost::beast::http::status::switching_protocols
            ? response_actions::upgrade
            : response.keep_alive()
                ? response_actions::keepalive
                : response_actions::close);
}

void wss::detail::http_client_servicer::close(
    const boost::system::error_code& ec)
{
    if (ec == boost::beast::error::timeout)
        return on_closed(ec);

    if (!stream.socket().is_open())
        return;

    boost::beast::error_code shutdown_ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, shutdown_ec);
    stream.close();

    on_closed(ec);
}
