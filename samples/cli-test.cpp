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

    auto c = std::make_shared<wss::client>(ioc.get_executor());

    for (unsigned i = 0; i < 1; ++i)
    {
        std::shared_ptr<wss::connection> conn;
        unsigned subscribed = 0;

        boost::asio::deadline_timer t(ioc);

        c->async_connect(
            "ws://localhost:7414",
            [&](const boost::system::error_code& ec, const std::shared_ptr<wss::connection>& connection)
            {
                if (ec)
                {
                    std::cout << "connect error: " << ec << std::endl;
                    t.cancel();
                    return;
                }

                std::cout << "connected!" << std::endl;
                conn = connection;

                connection->on_available.connect([&](const std::shared_ptr<wss::remote_signal>& signal)
                {
                    std::cout << "available: " << signal->id() << std::endl;

                    signal->on_subscribed.connect([]()
                    {
                        std::cout << "signal subscribed" << std::endl;
                    });

                    signal->on_data_received.connect([signal]()
                    {
                        std::cout << "signal data " << signal->id() << std::endl;
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

                    if (subscribed == 0 || subscribed == 2)
                    {
                        signal->subscribe();
                    }
                    ++subscribed;
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

        t.expires_from_now(boost::posix_time::seconds(5));
        t.async_wait([&](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            std::cout << "timer fired" << std::endl;
            std::cout << "stopping connection" << std::endl;
            if (conn)
                conn->stop();
            else
                c->cancel();
        });

        std::cout << "calling ioc.run()" << std::endl;
        ioc.restart();
        ioc.run();
        std::cout << "ioc.run() returned" << std::endl;
    }

    return 0;
}
