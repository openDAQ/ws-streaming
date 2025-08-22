#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
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

#include <ws-streaming/detail/http_client.hpp>

using namespace std::chrono_literals;
using namespace std::placeholders;

wss::detail::http_client::http_client(
        boost::asio::any_io_executor executor)
    : _resolver(executor)
    , _stream(executor)
{
}

void wss::detail::http_client::async_request(
    const std::string& hostname,
    const std::string& port,
    boost::beast::http::request<boost::beast::http::string_body>&& request,
    std::function<
        void(
            const boost::system::error_code& ec,
            const boost::beast::http::response<boost::beast::http::string_body>& response,
            boost::beast::tcp_stream& stream,
            const boost::beast::flat_buffer& buffer)
    > handler)
{
    _handler = std::move(handler);

    request.set(boost::beast::http::field::user_agent,
        "ws-streaming/" WS_STREAMING_VERSION_MAJOR
            "." WS_STREAMING_VERSION_MINOR
            "." WS_STREAMING_VERSION_PATCH
            " " BOOST_BEAST_VERSION_STRING);

    request.prepare_payload();
    _request = std::move(request);

    _resolver.async_resolve(
        hostname,
        port,
        std::bind(
            &http_client::finish_resolve,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2));
}

void wss::detail::http_client::cancel()
{
    _resolver.cancel();
    _stream.cancel();
}

void wss::detail::http_client::finish_resolve(
    const boost::system::error_code& ec,
    const boost::asio::ip::tcp::resolver::results_type& results)
{
    if (ec)
        return complete(ec);

    _stream.async_connect(
        results,
        std::bind(
            &http_client::finish_connect,
            shared_from_this(),
            std::placeholders::_1));
}

void wss::detail::http_client::finish_connect(
    const boost::system::error_code& ec)
{
    if (ec)
        return complete(ec);

    _stream.expires_after(std::chrono::seconds(30));

    boost::beast::http::async_write(
        _stream,
        _request,
        std::bind(
            &http_client::finish_write,
            shared_from_this(),
            std::placeholders::_1));
}

void wss::detail::http_client::finish_write(
    const boost::system::error_code& ec)
{
    if (ec)
        return complete(ec);

    _buffer.clear();

    boost::beast::http::async_read(
        _stream,
        _buffer,
        _response,
        std::bind(
            &http_client::finish_read,
            shared_from_this(),
            std::placeholders::_1));
}

void wss::detail::http_client::finish_read(
    const boost::system::error_code& ec)
{
    if (ec)
        return complete(ec);

    complete();
}

void wss::detail::http_client::complete(
    const boost::system::error_code& ec)
{
    if (!_handler)
        return;

    _handler(ec, _response, _stream, _buffer);
    _handler = {};
}
