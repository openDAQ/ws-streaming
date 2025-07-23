#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>

#include <ws-streaming/listener.hpp>

wss::listener::listener(
        boost::asio::strand<boost::asio::any_io_executor>& strand,
        boost::asio::ip::tcp::endpoint endpoint)
    : strand(strand)
    , acceptor(strand)
{
    boost::beast::error_code ec;

    acceptor.open(endpoint.protocol(), ec);
    if (ec)
        return;

    acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
        return;

    acceptor.bind(endpoint, ec);
    if (ec)
        return;

    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
        return;
}

wss::listener::~listener()
{
    std::cout << "listener destroyed" << std::endl;
}

void wss::listener::run()
{
    do_accept();
}

void wss::listener::stop()
{
    std::cout << "listener cancelling acceptor" << std::endl;
    acceptor.cancel();
}

void wss::listener::do_accept()
{
    acceptor.async_accept(
        strand,
        boost::beast::bind_front_handler(
            &listener::finish_accept,
            shared_from_this()));
}

void wss::listener::finish_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
{
    if (ec)
        return;

    on_accept(socket);
    do_accept();
}
