// Tests for establishing a connection with wss::client.

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <thread>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

#include <gtest/gtest.h>

#include <ws-streaming/client.hpp>
#include <ws-streaming/connection.hpp>

using boost::asio::ip::tcp;

// The server resets the connection right after the upgrade, so the socket has no peer address any more
TEST(Client, ReportsAConnectionResetRightAfterTheUpgrade)
{
    boost::asio::io_context server_ioc;
    tcp::acceptor acceptor{server_ioc, {boost::asio::ip::make_address("127.0.0.1"), 0}};
    std::promise<void> request_received;
    std::promise<void> client_paused;

    std::thread server{[&]
    {
        tcp::socket socket{server_ioc};
        acceptor.accept(socket);
        boost::asio::streambuf request;
        boost::asio::read_until(socket, request, "\r\n\r\n");
        request_received.set_value();
        client_paused.get_future().wait();

        boost::asio::write(socket, boost::asio::buffer(std::string{
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n\r\n"}));
        socket.set_option(tcp::socket::linger{true, 0});
        socket.close();
    }};

    boost::asio::io_context ioc;
    wss::client client{ioc.get_executor()};
    std::optional<boost::system::error_code> result;
    client.async_connect(
        "ws://127.0.0.1:" + std::to_string(acceptor.local_endpoint().port()) + "/",
        [&](const boost::system::error_code& ec, wss::connection_ptr) { result = ec; });

    // the client sends the request, then reads only once the response and the reset have both arrived
    auto received = request_received.get_future();
    while (received.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready)
        ioc.poll();
    client_paused.set_value();
    server.join();

    ioc.run_for(std::chrono::seconds(2));

    ASSERT_TRUE(result.has_value()) << "the connect handler was never called";
    EXPECT_TRUE(*result);
}
