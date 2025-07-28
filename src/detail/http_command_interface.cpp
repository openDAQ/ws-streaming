#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/http_command_interface.hpp>

using namespace std::chrono_literals;
using namespace std::placeholders;

wss::detail::http_command_interface::http_command_interface(
        boost::asio::any_io_executor executor,
        const std::string& hostname,
        const std::uint16_t port,
        const std::string& http_method,
        const std::string& path,
        const std::string& version)
    : _hostname(hostname)
    , _port(port)
    , _http_method(http_method)
    , _path(path)
    , _version(version)
    , _resolver(executor)
    , _stream(executor)
{
}

void wss::detail::http_command_interface::async_request(
    const std::string& method,
    const nlohmann::json& params,
    std::function<
        void(
            const boost::system::error_code& ec,
            const nlohmann::json& response)
    > handler)
{
    _handler = std::move(handler);

    prepare_request(method, params);

    _resolver.async_resolve(
        _hostname,
        std::to_string(_port),
        std::bind(
            &http_command_interface::finish_resolve,
            this,
            _1,
            _2));

}

void wss::detail::http_command_interface::cancel()
{
    _resolver.cancel();
    _stream.cancel();
}

void wss::detail::http_command_interface::prepare_request(
    const std::string& method,
    const nlohmann::json& params)
{
    _request = boost::beast::http::request<boost::beast::http::string_body>{
        boost::beast::http::verb::post /* @todo Use method specified in JSON */,
        _path,
        11 /* @todo Use HTTP version specified in JSON */};

    _request.set(boost::beast::http::field::content_type, "application/json");
    _request.set(boost::beast::http::field::host, _hostname);
    _request.set(boost::beast::http::field::user_agent,
        "ws-streaming/" WS_STREAMING_VERSION_MAJOR
            "." WS_STREAMING_VERSION_MINOR
            "." WS_STREAMING_VERSION_PATCH
            " " BOOST_BEAST_VERSION_STRING);

    _request.body() = nlohmann::json({
        { "jsonrpc", "2.0" },
        { "id", boost::uuids::to_string(_uuid_generator()) },
        { "method", method },
        { "params", params },
    }).dump();

    _request.prepare_payload();
}

void wss::detail::http_command_interface::finish_resolve(
    const boost::system::error_code& ec,
    const boost::asio::ip::tcp::resolver::results_type& results)
{
    std::cout << "http_command_interface finish_resolve(" << ec << ", ...)" << std::endl;

    if (ec)
        return _handler(ec, nullptr);

    std::cout << "http_command_interface calling async_connect()" << std::endl;
    _stream.async_connect(
        results,
        std::bind(
            &http_command_interface::finish_connect,
            this,
            _1));
}

void wss::detail::http_command_interface::finish_connect(
    const boost::system::error_code& ec)
{
    std::cout << "finish_connect(" << ec << ")" << std::endl;

    if (ec)
        return _handler(ec, nullptr);

    std::cout << "calling async_write()" << std::endl;
    _stream.expires_after(30s);

    boost::beast::http::async_write(
        _stream,
        _request,
        std::bind(
            &http_command_interface::finish_write,
            this,
            _1));
}

void wss::detail::http_command_interface::finish_write(
    const boost::system::error_code& ec)
{
    std::cout << "finish_write(" << ec << ")" << std::endl;

    if (ec)
        return _handler(ec, nullptr);

    std::cout << "calling async_read()" << std::endl;
    boost::beast::http::async_read(
        _stream,
        _buffer,
        _response,
        std::bind(
            &http_command_interface::finish_read,
            this,
            _1));
}

void wss::detail::http_command_interface::finish_read(
    const boost::system::error_code& ec)
{
    std::cout << "finish_read(" << ec << ")" << std::endl;

    if (ec)
        return _handler(ec, nullptr);

    nlohmann::json response;

    try
    {
        std::cout << "response body: " << _response.body() << std::endl;
        response = nlohmann::json::parse(_response.body());
    }

    catch (const nlohmann::json::exception& ex)
    {
        // @todo XXX TODO need better error code
        return _handler(boost::beast::http::error::unexpected_body, nullptr);
    }

    _handler({}, response);
}
