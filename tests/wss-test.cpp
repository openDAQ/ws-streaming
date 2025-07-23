#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio.hpp>

#include <ws-streaming/server.hpp>

static std::function<void()> do_exit;

int main(int argc, char *argv[])
{
    boost::asio::io_context ioc(1);
    wss::server server(ioc.get_executor());
    server.run();

    boost::asio::deadline_timer t(ioc);
    t.expires_from_now(boost::posix_time::seconds(600));
    t.async_wait([&](const boost::system::error_code& ec)
    {
        if (ec)
            return;
        std::cout << "timer fired" << std::endl;
        server.stop();
    });

    boost::asio::deadline_timer t2(ioc);
    auto do_broadcast = [&](const boost::system::error_code& ec)
    {
        if (ec)
            return;
        std::cout << "doing broadcast" << std::endl;
        server.debug_broadcast();
    };

    t2.expires_from_now(boost::posix_time::seconds(2));
    t2.async_wait(do_broadcast);

    do_exit = [&]()
    {
        std::cout << "interrupted, stopping server" << std::endl;
        server.stop();
        t.cancel();
        t2.cancel();
    };

    ::signal(SIGINT, [](int signal)
    {
        do_exit();
    });

    ioc.run();

    std::cout << "ioc.run() returned, exiting" << std::endl;

    return 0;
}
