#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio.hpp>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/transport/server.hpp>

static std::function<void()> do_exit;

int main(int argc, char *argv[])
{
    wss::local_signal ai0("ai0");

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

    do_exit = [&]()
    {
        std::cout << "interrupted, stopping server" << std::endl;
        server.stop();
        t.cancel();
    };

    ::signal(SIGINT, [](int signal)
    {
        do_exit();
    });

    ioc.run();

    std::cout << "ioc.run() returned, exiting" << std::endl;

    return 0;
}
