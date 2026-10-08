// Tests for choosing the command interface a server advertises.

#include <chrono>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/command_interface_client_factory.hpp>
#include <ws-streaming/detail/peer.hpp>

using boost::asio::ip::tcp;

// The server may advertise the port of its HTTP JSON-RPC interface as a number.
TEST(CommandInterfaceClientFactory, SendsRequestsToANumericJsonRpcHttpPort)
{
    boost::asio::io_context ioc;
    auto loopback = boost::asio::ip::make_address("127.0.0.1");

    tcp::acceptor streaming_acceptor{ioc, {loopback, 0}};
    tcp::socket streaming_socket{ioc};
    streaming_socket.connect(streaming_acceptor.local_endpoint());
    tcp::socket streaming_server_side = streaming_acceptor.accept();
    auto peer = std::make_shared<wss::detail::peer>(std::move(streaming_socket), true);

    tcp::acceptor http_acceptor{ioc, {loopback, 0}};
    bool accepted = false;
    http_acceptor.async_accept([&](const boost::system::error_code& ec, tcp::socket) { accepted = !ec; });

    nlohmann::json interfaces = {{"jsonrpc-http", {
        {"httpMethod", "POST"},
        {"httpPath", "/"},
        {"httpVersion", "1.1"},
        {"port", http_acceptor.local_endpoint().port()}}}};

    auto client = wss::detail::command_interface_client_factory::create_client(interfaces, peer);
    ASSERT_NE(client, nullptr);
    client->async_request("subscribe", nlohmann::json::array({"signal"}), [](auto&&, auto&&) { });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!accepted && ioc.run_one_until(deadline)) { }

    EXPECT_TRUE(accepted) << "no request reached the advertised port";
    client->cancel();
}
