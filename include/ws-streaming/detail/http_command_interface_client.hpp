#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/detail/http_client.hpp>

namespace wss::detail
{
    /**
     * A client for the "jsonrpc-http" command interface. Requests are sent one at a time, each
     * after the previous one is answered, so the peer runs them in call order. A request without
     * an answer (a connection error, or no response within 2 s) is sent once more, then dropped.
     * Any HTTP response, whatever its status or body, is final.
     */
    class http_command_interface_client
        : public command_interface_client
    {
        public:

            http_command_interface_client(
                boost::asio::any_io_executor executor,
                const std::string& hostname,
                const std::string& port,
                const std::string& http_method,
                const std::string& path,
                const std::string& version);

            ~http_command_interface_client() override;

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

            struct request
            {
                std::string body;
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const nlohmann::json& response)
                > handler;
                unsigned attempts = 0;
            };

            void send_front();
            void on_no_answer(const boost::system::error_code& ec);
            void complete_front(const boost::system::error_code& ec, const nlohmann::json& response);

        private:

            boost::asio::any_io_executor _executor;
            std::string _hostname;
            std::string _port;
            std::string _http_method;
            std::string _path;
            std::string _version;
            unsigned _next_id = 1;

            std::deque<request> _requests;                  // the front one is in flight
            std::shared_ptr<detail::http_client> _client;   // the attempt in flight, set while _requests is not empty
            boost::asio::steady_timer _timer;               // times out the attempt in flight
            std::shared_ptr<int> _lifetime = std::make_shared<int>();  // expires with this object; a handler queued before that checks it first
    };
}
