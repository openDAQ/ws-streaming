#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>

#include <ws-streaming/client.hpp>
#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/base64.hpp>
#include <ws-streaming/detail/url.hpp>

wss::client::client(boost::asio::any_io_executor executor)
    : _http_client{std::make_shared<detail::http_client>(executor)}
    , _executor(executor)
{
}

void wss::client::async_connect(
    std::string_view url,
    std::function<
        void(
            const boost::system::error_code& ec,
            connection_ptr connection)
    > handler)
{
    detail::url url_obj{url};

    if (url_obj.scheme() == "ws")
    {
        std::uint16_t port = url_obj.port_number()
            .value_or(detail::streaming_protocol::DEFAULT_WEBSOCKET_PORT);

        _http_client->async_request(
            url_obj.host_address(),
            std::to_string(port),
            create_request(url),
            [handler = std::move(handler)](
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
                    stream.release_socket(),
                    true);

                auto data = buffer.data();
                connection->run(data.data(), data.size());

                handler({}, connection);
            });
    }

    else
    {
        std::uint16_t port = url_obj.port_number()
            .value_or(detail::streaming_protocol::DEFAULT_TCP_PORT);

        auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(_executor);

        resolver->async_resolve(
            url_obj.host_address(),
            std::to_string(port),
            [
                resolver,
                handler = std::move(handler)
            ](
                const boost::system::error_code& ec,
                const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return handler(ec, {});

                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(resolver->get_executor());

                socket->async_connect(
                    *results.begin(),
                    [socket, handler = std::move(handler)]
                    (const boost::system::error_code& ec)
                    {
                        if (ec)
                            return handler(ec, {});

                        auto connection = std::make_shared<wss::connection>(
                            std::move(*socket),
                            true,
                            true);

                        connection->run();
                        handler({}, connection);
                    });
            });
    }
}

void wss::client::cancel()
{
    _http_client->cancel();
}

boost::beast::http::request<boost::beast::http::string_body>
wss::client::create_request(const detail::url& url)
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
    request.set(boost::beast::http::field::sec_websocket_version, "13");
    request.set(boost::beast::http::field::upgrade, "websocket");

    return request;
}

std::string wss::client::get_random_key()
{
    auto now = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    now.resize(16, '\0');
    return detail::base64(now);
}
