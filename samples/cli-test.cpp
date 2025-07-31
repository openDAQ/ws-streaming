#include <cstddef>
#include <cstdint>
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
    std::shared_ptr<wss::remote_signal> subscribed_signal1;
    std::shared_ptr<wss::remote_signal> subscribed_signal2;

    auto c = std::make_shared<wss::client>(ioc.get_executor());

    for (unsigned i = 0; i < 1; ++i)
    {
        std::shared_ptr<wss::connection> conn;

        boost::asio::deadline_timer t(ioc);
        boost::asio::deadline_timer t2(ioc);
        boost::asio::deadline_timer t3(ioc);
        boost::asio::deadline_timer t4(ioc);

        c->async_connect(
            "ws://localhost:7414",
            [&](const boost::system::error_code& ec, const std::shared_ptr<wss::connection>& connection)
            {
                if (ec)
                {
                    std::cout << "connect error: " << ec << std::endl;
                    t.cancel();
                    t2.cancel();
                    t3.cancel();
                    t4.cancel();
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

                    signal->on_data_received.connect([signal](std::int64_t, const void *, std::size_t)
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

                    if (signal->id() == "/Value1")
                    {
                        signal->subscribe();
                        subscribed_signal1 = signal;
                    }

                    if (signal->id() == "/Value2")
                    {
                        signal->subscribe();
                        subscribed_signal2 = signal;
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

        t.expires_from_now(boost::posix_time::seconds(10));
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

        t2.expires_from_now(boost::posix_time::seconds(3));
        t2.async_wait([&](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            std::cout << "timer 2 fired" << std::endl;
            std::cout << "unsubscribing" << std::endl;
            if (subscribed_signal1)
                subscribed_signal1->unsubscribe();
//            if (subscribed_signal2)
//                subscribed_signal2->unsubscribe();
        });

        t3.expires_from_now(boost::posix_time::seconds(6));
        t3.async_wait([&](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            std::cout << "timer 3 fired" << std::endl;
            std::cout << "subscribing" << std::endl;
            if (subscribed_signal1)
                subscribed_signal1->subscribe();
//            if (subscribed_signal2)
//                subscribed_signal2->subscribe();
        });

        t4.expires_from_now(boost::posix_time::seconds(9));
        t4.async_wait([&](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            std::cout << "timer 4 fired" << std::endl;
            std::cout << "unsubscribing" << std::endl;
            if (subscribed_signal1)
                subscribed_signal1->unsubscribe();
            if (subscribed_signal2)
                subscribed_signal2->unsubscribe();
            subscribed_signal1.reset();
            subscribed_signal2.reset();
        });

        std::cout << "calling ioc.run()" << std::endl;
        ioc.restart();
        ioc.run();
        std::cout << "ioc.run() returned" << std::endl;
    }
}
