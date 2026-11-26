// Tests for wss::detail::http_client.

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

#include <gtest/gtest.h>

#include <ws-streaming/detail/http_client.hpp>

using namespace std::chrono_literals;

TEST(HttpClient, CancelBetweenTwoStepsEndsTheRequest)
{
    boost::asio::io_context ioc{1};
    boost::asio::ip::tcp::acceptor acceptor{ioc, {boost::asio::ip::make_address("127.0.0.1"), 0}};

    auto client = std::make_shared<wss::detail::http_client>(ioc.get_executor());
    std::optional<boost::system::error_code> result;

    client->async_request("127.0.0.1", std::to_string(acceptor.local_endpoint().port()),
        boost::beast::http::request<boost::beast::http::string_body>{boost::beast::http::verb::post, "/", 11},
        [&result](const boost::system::error_code& ec, const auto&, auto&, const auto&) { result = ec; });

    // the resolve completes and starts the connect; once the connection is accepted, the connect's completion is
    // queued, so the cancel comes between the connect and the write
    ioc.run_one();
    boost::asio::ip::tcp::socket peer{ioc};
    acceptor.accept(peer);
    client->cancel();
    ioc.run_for(10s);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, boost::asio::error::operation_aborted);
    EXPECT_EQ(peer.available(), 0u) << "the request was written after it was canceled";
}
