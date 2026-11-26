// Tests for publishing signals with wss::server.

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include <gtest/gtest.h>

#include <ws-streaming/client.hpp>
#include <ws-streaming/connection.hpp>
#include <ws-streaming/data_types.hpp>
#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/metadata_builder.hpp>
#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/server.hpp>

using boost::asio::ip::tcp;

namespace
{
    std::uint16_t free_port()
    {
        boost::asio::io_context ioc;
        tcp::acceptor acceptor{ioc, {boost::asio::ip::make_address("127.0.0.1"), 0}};
        return acceptor.local_endpoint().port();
    }

    template <typename Predicate>
    bool run_until(boost::asio::io_context& ioc, Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate() && std::chrono::steady_clock::now() < deadline)
            ioc.run_for(std::chrono::milliseconds(10));
        return predicate();
    }
}

// A write that fails closes its client at once, while the server goes through its clients to announce a signal
TEST(Server, AnnouncesASignalPastAClientWhoseWriteFails)
{
    boost::asio::io_context ioc{1};
    wss::local_signal signal{"/Value", wss::metadata_builder{"Value"}.data_type(wss::data_types::real64_t).build()};
    std::size_t connected = 0;
    wss::connection_ptr client_connection;
    bool announced = false;

    const auto port = free_port();
    wss::server server{ioc.get_executor()};
    server.add_listener(port);
    server.run();
    server.on_client_connected.connect([&](wss::connection_ptr&) { ++connected; });

    // the first client is a bare socket, so that the test can reset it
    tcp::socket broken{ioc};
    broken.connect({boost::asio::ip::make_address("127.0.0.1"), port});
    boost::asio::write(broken, boost::asio::buffer(std::string{
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"}));
    ASSERT_TRUE(run_until(ioc, [&] { return connected == 1; }));

    wss::client client{ioc.get_executor()};
    client.async_connect("ws://127.0.0.1:" + std::to_string(port) + "/",
        [&](const boost::system::error_code& ec, wss::connection_ptr connection)
        {
            if (ec)
                return;
            client_connection = connection;
            client_connection->on_available.connect([&](wss::remote_signal_ptr remote)
            {
                if (remote->id() == signal.id())
                    announced = true;
            });
        });
    ASSERT_TRUE(run_until(ioc, [&] { return connected == 2 && client_connection; }));

    // the reset reaches the server's socket while the I/O context is not run, so the server's next write to it fails
    broken.set_option(tcp::socket::linger{true, 0});
    broken.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server.add_local_signal(signal);
    EXPECT_TRUE(run_until(ioc, [&] { return announced; }));

    server.remove_local_signal(signal);
    client_connection->close();
    server.close();
    ioc.run_for(std::chrono::milliseconds(100));
}
