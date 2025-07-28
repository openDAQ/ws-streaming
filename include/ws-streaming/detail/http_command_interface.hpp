#pragma once

#include <cstdint>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/random_generator.hpp>

#include <ws-streaming/detail/command_interface.hpp>

namespace wss::detail
{
    class http_command_interface
        : public command_interface
    {
        public:

            http_command_interface(
                boost::asio::any_io_executor executor,
                const std::string& hostname,
                const std::uint16_t port,
                const std::string& http_method,
                const std::string& path,
                const std::string& version);

            void async_request(
                const std::string& method,
                const nlohmann::json& params,
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const nlohmann::json& response)
                > handler) override;

            void cancel() override;

        private:

            void prepare_request(
                const std::string& method,
                const nlohmann::json& params);

            void finish_resolve(
                const boost::system::error_code& ec,
                const boost::asio::ip::tcp::resolver::results_type& results);

            void finish_connect(
                const boost::system::error_code& ec);

            void finish_write(
                const boost::system::error_code& ec);

            void finish_read(
                const boost::system::error_code& ec);

        private:

            std::string _hostname;
            std::uint16_t _port;
            std::string _http_method;
            std::string _path;
            std::string _version;

            boost::uuids::random_generator _uuid_generator;

            boost::asio::ip::tcp::resolver _resolver;
            boost::beast::tcp_stream _stream;
            boost::beast::http::request<boost::beast::http::string_body> _request;
            boost::beast::flat_buffer _buffer;
            boost::beast::http::response<boost::beast::http::string_body> _response;

            std::function<void(
                const boost::system::error_code& ec,
                const nlohmann::json& response)
            > _handler;
    };
}
