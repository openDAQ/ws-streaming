#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/signals2/signal.hpp>

#include <ws-streaming/session.hpp>

namespace wss
{
    class listener : public std::enable_shared_from_this<listener>
    {
        public:

            listener(
                boost::asio::strand<boost::asio::any_io_executor>& strand,
                boost::asio::ip::tcp::endpoint endpoint);

            ~listener();

            void run();
            void stop();

            boost::signals2::signal<void(boost::asio::ip::tcp::socket&)> on_accept;

        private:

            void do_accept();

            void finish_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

            boost::asio::strand<boost::asio::any_io_executor>& strand;
            boost::asio::ip::tcp::acceptor acceptor;
    };
}
