#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <boost/url.hpp>

#include <ws-streaming/client.hpp>
#include <ws-streaming/detail/base64.hpp>

using namespace std::chrono_literals;
using namespace std::placeholders;

wss::client::client(boost::asio::any_io_executor executor)
    : _resolver{executor}
    , _stream{executor}
{
}

void wss::client::connect(const boost::urls::url_view& url)
{
    std::uint16_t port = url.port_number();

    if (!port)
        port = 80;

    _hostname = url.host_address();
    prepare_request(url.path());

    std::cout << "calling async_resolve()" << std::endl;
    _resolver.async_resolve(
        url.host_address(),
        std::to_string(url.port_number()),
        std::bind(
            &client::finish_resolve,
            shared_from_this(),
            _1,
            _2));
}

void wss::client::prepare_request(
    const std::string& path)
{
    _request = boost::beast::http::request<boost::beast::http::string_body>{
        boost::beast::http::verb::get,
        path,
        11};

    _request.set(boost::beast::http::field::connection, "Upgrade");
    _request.set(boost::beast::http::field::host, _hostname);
    _request.set(boost::beast::http::field::sec_websocket_key, get_random_key());
    _request.set(boost::beast::http::field::upgrade, "websocket");
    _request.set(boost::beast::http::field::user_agent,
        "ws-streaming/" WS_STREAMING_VERSION_MAJOR
            "." WS_STREAMING_VERSION_MINOR
            "." WS_STREAMING_VERSION_PATCH
            " " BOOST_BEAST_VERSION_STRING);
}

void wss::client::finish_resolve(
    const boost::system::error_code& ec,
    const boost::asio::ip::tcp::resolver::results_type& results)
{
    std::cout << "finish_resolve(" << ec << ", ...)" << std::endl;

    if (ec)
        return on_error(ec);

    std::cout << "calling async_connect()" << std::endl;
    _stream.async_connect(
        results,
        std::bind(
            &client::finish_connect,
            shared_from_this(),
            _1));
}

void wss::client::finish_connect(
    const boost::system::error_code& ec)
{
    std::cout << "finish_connect(" << ec << ")" << std::endl;

    if (ec)
        return on_error(ec);

    std::cout << "calling async_write()" << std::endl;
    _stream.expires_after(30s);

    boost::beast::http::async_write(
        _stream,
        _request,
        std::bind(
            &client::finish_write,
            shared_from_this(),
            _1));
}

void wss::client::finish_write(
    const boost::system::error_code& ec)
{
    std::cout << "finish_write(" << ec << ")" << std::endl;

    if (ec)
        return on_error(ec);

    std::cout << "calling async_read()" << std::endl;
    boost::beast::http::async_read(
        _stream,
        _buffer,
        _response,
        std::bind(
            &client::finish_read,
            shared_from_this(),
            _1));
}

void wss::client::finish_read(
    const boost::system::error_code& ec)
{
    std::cout << "finish_read(" << ec << ")" << std::endl;

    if (ec)
        return on_error(ec);

    if (_response.result() != boost::beast::http::status::switching_protocols)
        return on_error(boost::beast::http::error::bad_status);

    auto socket = _stream.release_socket();
    auto connection = std::make_shared<wss::connection>(
        _hostname,
        std::move(socket),
        true);

    auto data = _buffer.data();
    connection->run(data.data(), data.size());
    on_connected(connection);
}

std::string wss::client::get_random_key()
{
    auto now = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    now.resize(16, '\0');
    return detail::base64(now);
}
