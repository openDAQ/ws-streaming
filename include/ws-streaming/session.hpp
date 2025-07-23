#pragma once

#include <cstddef>
#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

namespace wss
{
    class session : public std::enable_shared_from_this<session>
    {
        public:

            session(boost::asio::ip::tcp::socket&& socket);

            ~session();

            void run();

            void stop();

            boost::signals2::signal<void(boost::asio::ip::tcp::socket&)> on_websocket_connection;
            boost::signals2::signal<nlohmann::json(const nlohmann::json&)> on_control_request;

        private:

            void do_read();

            void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);

            enum class response_actions
            {
                close,
                keepalive,
                upgrade
            };

            void send_response(
                boost::beast::http::message_generator&& msg,
                response_actions action);

            void on_write(
                response_actions action,
                boost::beast::error_code ec,
                std::size_t bytes_transferred);

            void do_close();

            boost::beast::tcp_stream stream;
            boost::beast::flat_buffer buffer;
            boost::beast::http::request<boost::beast::http::string_body> req;
    };
}
