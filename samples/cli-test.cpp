#include <iostream>
#include <memory>
#include <utility>

#include <ws-streaming/client.hpp>
#include <ws-streaming/connection.hpp>
#include <ws-streaming/remote_signal.hpp>

#include <boost/asio.hpp>

int main(int argc, char *argv[])
{
    boost::asio::io_context ioc(1);
    std::shared_ptr<wss::connection> conn;
    bool subscribed = false;

    auto c = std::make_shared<wss::client>(ioc.get_executor());
    c->connect("ws://localhost:7414/");

    c->on_error.connect([](const boost::system::error_code& ec)
    {
        std::cout << "connect error: " << ec << std::endl;
    });

    c->on_connected.connect([&](const std::shared_ptr<wss::connection>& connection)
    {
        std::cout << "connected!" << std::endl;
        conn = connection;

        connection->on_available.connect([&](const std::shared_ptr<wss::remote_signal>& signal)
        {
            std::cout << "available: " << signal->id() << std::endl;

            signal->on_subscribed.connect([]()
            {
                std::cout << "signal subscribed" << std::endl;
            });

            signal->on_data_received.connect([]()
            {
                std::cout << "signal data" << std::endl;
            });

            signal->on_metadata_changed.connect([]()
            {
                std::cout << "signal metadata changed" << std::endl;
            });

            signal->on_unsubscribed.connect([]()
            {
                std::cout << "signal unsubscribed" << std::endl;
            });

            signal->on_unavailable.connect([]()
            {
                std::cout << "signal unavailable" << std::endl;
            });

            if (!subscribed)
            {
                subscribed = true;
                signal->subscribe();
            }
        });

        connection->on_unavailable.connect([](const std::shared_ptr<wss::remote_signal>& signal)
        {
            std::cout << "unavailable: " << signal->id() << std::endl;
        });

        connection->on_disconnected.connect([]()
        {
            std::cout << "disconnected" << std::endl;
        });
    });

    boost::asio::deadline_timer t(ioc);
    t.expires_from_now(boost::posix_time::seconds(5));
    t.async_wait([&](const boost::system::error_code& ec)
    {
        if (ec)
            return;
        std::cout << "timer fired" << std::endl;
        if (conn)
        {
            std::cout << "stopping connection" << std::endl;
            conn->stop();
        }
    });

    std::cout << "calling ioc.run()" << std::endl;
    ioc.run();
    std::cout << "ioc.run() returned" << std::endl;
}
