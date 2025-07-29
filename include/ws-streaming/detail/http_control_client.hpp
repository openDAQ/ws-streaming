#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/control_client.hpp>
#include <ws-streaming/detail/http_client.hpp>

namespace wss::detail
{
    class http_control_client
        : public control_client
    {
        public:

            http_control_client(
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

        private:

            boost::asio::any_io_executor _executor;
            std::string _hostname;
            std::uint16_t _port;
            std::string _http_method;
            std::string _path;
            std::string _version;
            unsigned _next_id = 1;

            std::set<std::shared_ptr<detail::http_client>> _clients;
    };
}
