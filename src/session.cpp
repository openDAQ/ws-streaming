#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include <ws-streaming/session.hpp>
#include <ws-streaming/websocket_protocol.hpp>

template <typename Container>
static std::string base64(const Container& val)
{
    auto begin = std::begin(val);
    auto end = std::end(val);
    using namespace boost::archive::iterators;
    using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
    auto tmp = std::string(It(begin), It(end));
    return tmp.append((3 - (end - begin) % 3) % 3, '=');
}

wss::session::session(boost::asio::ip::tcp::socket&& socket)
    : stream(std::move(socket))
{
}

wss::session::~session()
{
    std::cout << "session destroyed" << std::endl;
}

void wss::session::run()
{
    boost::asio::dispatch(
        stream.get_executor(),
        boost::beast::bind_front_handler(
            &session::do_read,
            shared_from_this()));
}

void wss::session::stop()
{
    std::cout << "session cancelling stream" << std::endl;
    stream.cancel();
}

void wss::session::do_read()
{
    stream.expires_after(std::chrono::seconds(30));

    boost::beast::http::async_read(
        stream,
        buffer,
        req = {},
        boost::beast::bind_front_handler(
            &session::on_read,
            shared_from_this()));
}

void wss::session::on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec == boost::beast::http::error::end_of_stream)
        return do_close();

    if (ec)
        return;

    auto get_header = [&](boost::beast::http::field field) -> std::string
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
        boost::uuids::detail::sha1 sha1;
        sha1.process_bytes(key.data(), key.length());
        sha1.process_bytes(websocket_protocol::MAGIC_KEY, sizeof(websocket_protocol::MAGIC_KEY) - 1);

        boost::uuids::detail::sha1::digest_type sha1_value;
        sha1.get_digest(sha1_value);

        std::array<char, 20> sha1_bytes;
        for (unsigned i = 0; i < 5; ++i)
        {
            sha1_bytes[4 * i + 0] = sha1_value[i] >> 24;
            sha1_bytes[4 * i + 1] = sha1_value[i] >> 16;
            sha1_bytes[4 * i + 2] = sha1_value[i] >> 8;
            sha1_bytes[4 * i + 3] = sha1_value[i];
        }

        auto response_key = base64(sha1_bytes);

        boost::beast::http::response<boost::beast::http::string_body> res(
            boost::beast::http::status::switching_protocols,
            req.version());

        res.set(boost::beast::http::field::server, "ws-streaming/1.0 " BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::connection, "Upgrade");
        res.set(boost::beast::http::field::upgrade, "websocket");
        res.set(boost::beast::http::field::sec_websocket_accept, response_key);
        res.prepare_payload();

        send_response(std::move(res), response_actions::upgrade);
    }

    else if (req.method() == boost::beast::http::verb::post)
    {
        nlohmann::json request_json;
        auto response_json = on_control_request(request_json);

        if (response_json.has_value())
        {
            boost::beast::http::response<boost::beast::http::string_body> res(
                boost::beast::http::status::ok,
                req.version());

            res.set(boost::beast::http::field::server, "ws-streaming/1.0 " BOOST_BEAST_VERSION_STRING);
            res.keep_alive(req.keep_alive());

            res.set(boost::beast::http::field::content_type, "application/json");
            res.body() = response_json.value().dump();
            res.prepare_payload();
            send_response(std::move(res), req.keep_alive() ? response_actions::keepalive : response_actions::close);
        }

        else
        {
            boost::beast::http::response<boost::beast::http::string_body> res(
                boost::beast::http::status::internal_server_error,
                req.version());

            res.set(boost::beast::http::field::server, "ws-streaming/1.0 " BOOST_BEAST_VERSION_STRING);
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
            send_response(std::move(res), req.keep_alive() ? response_actions::keepalive : response_actions::close);
        }
    }

    else
    {
        boost::beast::http::response<boost::beast::http::string_body> res(
            boost::beast::http::status::bad_request,
            req.version());

        res.set(boost::beast::http::field::server, "ws-streaming/1.0 " BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string("Unrecognized request");
        res.prepare_payload();

        send_response(std::move(res), req.keep_alive() ? response_actions::keepalive : response_actions::close);
    }
}

void wss::session::send_response(
    boost::beast::http::message_generator&& msg,
    response_actions action)
{
    boost::beast::async_write(
        stream,
        std::move(msg),
        boost::beast::bind_front_handler(
            &session::on_write,
            shared_from_this(),
            action));
}

void wss::session::on_write(
    response_actions action,
    boost::beast::error_code ec,
    std::size_t bytes_transferred)
{
    if (ec)
        return;

    switch (action)
    {
        case response_actions::keepalive:
            return do_read();

        case response_actions::upgrade:
        {
            auto socket = stream.release_socket();
            return on_websocket_connection(socket);
        }

        default:
            return do_close();
    }
}

void wss::session::do_close()
{
    boost::beast::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}
