#include <chrono>
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
#include <boost/url.hpp>

#include <ws-streaming/client.hpp>
#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/base64.hpp>

wss::client::client(boost::asio::any_io_executor executor)
    : _http_client{std::make_shared<detail::http_client>(executor)}
{
}

void wss::client::async_connect(
    const boost::urls::url_view& url,
    std::function<
        void(
            const boost::system::error_code& ec,
            const std::shared_ptr<wss::connection>& connection)
    > handler)
{
    std::uint16_t port = url.port_number();
    if (!port)
        port = 80;

    _http_client->async_request(
        url.host_address(),
        port,
        create_request(url),
        [handler = std::move(handler), hostname = url.host_address()](
            const boost::system::error_code& ec,
            const boost::beast::http::response<boost::beast::http::string_body>& response,
            boost::beast::tcp_stream& stream,
            const boost::beast::flat_buffer& buffer)
        {
            if (ec)
                return handler(ec, {});

            if (response.result() != boost::beast::http::status::switching_protocols)
                return handler(boost::beast::http::error::bad_status, {});

            auto connection = std::make_shared<wss::connection>(
                hostname,
                stream.release_socket(),
                true);

            auto data = buffer.data();
            connection->run(data.data(), data.size());

            handler({}, connection);
        });
}

void wss::client::cancel()
{
    _http_client->cancel();
}

boost::beast::http::request<boost::beast::http::string_body>
wss::client::create_request(const boost::urls::url_view& url)
{
    std::string path = url.path();
    if (path.empty())
        path = "/";

    boost::beast::http::request<boost::beast::http::string_body> request{
        boost::beast::http::verb::get,
        path,
        11};

    request.set(boost::beast::http::field::connection, "Upgrade");
    request.set(boost::beast::http::field::host, url.host_address());
    request.set(boost::beast::http::field::sec_websocket_key, get_random_key());
    request.set(boost::beast::http::field::upgrade, "websocket");

    return request;
}

std::string wss::client::get_random_key()
{
    auto now = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    now.resize(16, '\0');
    return detail::base64(now);
}
