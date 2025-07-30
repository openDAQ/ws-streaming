#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
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
        const std::uint16_t port,
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

void wss::detail::http_command_interface_client::async_request(
    const std::string& method,
    const nlohmann::json& params,
    std::function<
        void(
            const boost::system::error_code& ec,
            const nlohmann::json& response)
    > handler)
{
    boost::beast::http::request<boost::beast::http::string_body> request{
        boost::beast::http::verb::post /* @todo Use method specified in JSON */,
        _path,
        11 /* @todo Use HTTP version specified in JSON */};

    request.set(boost::beast::http::field::content_type, "application/json");
    request.set(boost::beast::http::field::host, _hostname);

    request.body() = nlohmann::json({
        { "jsonrpc", "2.0" },
        { "id", _next_id++ },
        { "method", method },
        { "params", params },
    }).dump();

    auto client = std::make_shared<detail::http_client>(_executor);

    client->async_request(_hostname, _port, std::move(request),
        [this, handler = std::move(handler), client](
            const boost::system::error_code& ec,
            const boost::beast::http::response<boost::beast::http::string_body>& response,
            boost::beast::tcp_stream& stream,
            const boost::beast::flat_buffer& buffer)
        {
            _clients.erase(client);

            if (ec)
                return handler(ec, nullptr);

            if (response.result_int() < 200 || response.result_int() >= 400)
                return handler(boost::beast::http::error::bad_status, nullptr);

            nlohmann::json response_json;

            try
            {
                response_json = nlohmann::json::parse(response.body());
            }

            catch (const nlohmann::json::exception& ex)
            {
                // @todo XXX TODO need better error code
                return handler(boost::beast::http::error::unexpected_body, nullptr);
            }

            handler({}, response_json);
        });

    _clients.emplace(std::move(client));
}

void wss::detail::http_command_interface_client::cancel()
{
    for (const auto& client : _clients)
        client->cancel();
    _clients.clear();
}
