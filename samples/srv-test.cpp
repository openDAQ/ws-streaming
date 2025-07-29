#include <chrono>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio.hpp>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/transport/server.hpp>

using namespace std::chrono_literals;

static std::function<void()> do_exit;

static wss::local_signal ai0("ai0");

void tick(
    boost::asio::steady_timer& dt,
    const boost::system::error_code& ec)
{
    if (ec)
        return;

    ai0.on_data("hello", 5);
    dt.expires_after(100ms);
    dt.async_wait(std::bind(tick, std::ref(dt), std::placeholders::_1));
}

int main(int argc, char *argv[])
{
    boost::asio::io_context ioc(1);
    wss::transport::server server(ioc.get_executor());
    server.add_signal(ai0);
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
    t2.expires_from_now(boost::posix_time::seconds(5));
    t2.async_wait([&](const boost::system::error_code& ec)
    {
        if (ec)
            return;
        std::cout << "timer 2 fired" << std::endl;
        ai0.set_metadata({ { "description", true } });
    });

    boost::asio::steady_timer dt(ioc);
    dt.expires_after(100ms);
    dt.async_wait(std::bind(tick, std::ref(dt), std::placeholders::_1));

    do_exit = [&]()
    {
        std::cout << "interrupted, stopping server" << std::endl;
        server.stop();
        t.cancel();
        dt.cancel();
    };

    ::signal(SIGINT, [](int signal)
    {
        do_exit();        
    });

    ioc.run();

    std::cout << "ioc.run() returned, exiting" << std::endl;

    return 0;
}
