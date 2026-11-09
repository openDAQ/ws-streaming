// Tests for wss::detail::http_client.

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

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

    // a port that listens but never accepts, so a request that goes on connects and waits for an answer
    boost::asio::ip::tcp::acceptor acceptor{ioc, {boost::asio::ip::make_address("127.0.0.1"), 0}};

    auto client = std::make_shared<wss::detail::http_client>(ioc.get_executor());
    std::optional<boost::system::error_code> result;

    client->async_request("127.0.0.1", std::to_string(acceptor.local_endpoint().port()),
        boost::beast::http::request<boost::beast::http::string_body>{boost::beast::http::verb::post, "/", 11},
        [&result](const boost::system::error_code& ec, const auto&, auto&, const auto&) { result = ec; });

    // the resolver finishes on its own thread and queues its handler, which waits for ioc to run; if it takes
    // longer than that, the cancel reaches it in time and the test passes without exercising a late cancel
    std::this_thread::sleep_for(200ms);
    client->cancel();
    ioc.run_for(10s);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, boost::asio::error::operation_aborted);

    // the next step, the connect, never started
    acceptor.non_blocking(true);
    boost::system::error_code accept_ec;
    acceptor.accept(accept_ec);
    EXPECT_EQ(accept_ec, boost::asio::error::would_block) << "the request went on to connect after it was canceled";
}
