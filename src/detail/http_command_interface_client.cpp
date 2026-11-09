#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/version.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/http_client.hpp>
#include <ws-streaming/detail/http_command_interface_client.hpp>

wss::detail::http_command_interface_client::http_command_interface_client(
        boost::asio::any_io_executor executor,
        const std::string& hostname,
        const std::string& port,
        const std::string& http_method,
        const std::string& path,
        const std::string& version)
    : _executor(executor)
    , _hostname(hostname)
    , _port(port)
    , _http_method(http_method)
    , _path(path)
    , _version(version)
{
}

wss::detail::http_command_interface_client::~http_command_interface_client()
{
    // the request in flight lives until its handler runs; the cancel makes that handler return at once
    if (_client)
        _client->cancel();
}

void wss::detail::http_command_interface_client::async_request(
    const std::string& method,
    const nlohmann::json& params,
    std::function<
        void(
            const boost::system::error_code& ec,
            const nlohmann::json& response)
    > handler)
{
    _requests.push_back({
        nlohmann::json({
            { "jsonrpc", "2.0" },
            { "id", _next_id++ },
            { "method", method },
            { "params", params },
        }).dump(),
        std::move(handler)});

    if (!_client)
        send_front();
}

void wss::detail::http_command_interface_client::cancel()
{
    if (_client)
        std::exchange(_client, nullptr)->cancel();

    decltype(_requests) old_requests;
    std::swap(_requests, old_requests);

    for (const auto& request : old_requests)
        request.handler(boost::asio::error::operation_aborted, nullptr);
}

void wss::detail::http_command_interface_client::send_front()
{
    boost::beast::http::request<boost::beast::http::string_body> request{
        boost::beast::http::verb::post /* @todo Use method specified in JSON */,
        _path,
        11 /* @todo Use HTTP version specified in JSON */};

    request.set(boost::beast::http::field::content_type, "application/json");
    request.set(boost::beast::http::field::host, _hostname);
    request.body() = _requests.front().body;

    auto client = std::make_shared<detail::http_client>(_executor);
    _client = client;

    // the handler owns the client until it runs, so a canceled request can complete safely
    client->async_request(_hostname, _port, std::move(request),
        [this, client, lifetime = std::weak_ptr<int>(_lifetime)](
            const boost::system::error_code& ec,
            const boost::beast::http::response<boost::beast::http::string_body>& response,
            boost::beast::tcp_stream& /*stream*/,
            const boost::beast::flat_buffer& /*buffer*/)
        {
            if (lifetime.expired() || ec == boost::asio::error::operation_aborted || client != _client)
                return;

            if (ec)
                return complete_front(ec, nullptr);

            if (response.result_int() < 200 || response.result_int() >= 400)
                return complete_front(boost::beast::http::error::bad_status, nullptr);

            nlohmann::json response_json;

            try
            {
                response_json = nlohmann::json::parse(response.body());
            }

            catch (const nlohmann::json::exception& /*ex*/)
            {
            }

            complete_front({}, response_json);
        });
}

void wss::detail::http_command_interface_client::complete_front(
    const boost::system::error_code& ec,
    const nlohmann::json& response)
{
    _client.reset();

    auto handler = std::move(_requests.front().handler);
    _requests.pop_front();

    // the next request goes before the handler runs, so a request the handler makes queues behind it
    if (!_requests.empty())
        send_front();

    handler(ec, response);
}
