#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace wss::detail
{
    class control_client
    {
        public:

            virtual ~control_client()
            {
            }

            virtual void async_request(
                const std::string& method,
                const nlohmann::json& params,
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const nlohmann::json& response)
                > handler) = 0;

            virtual void cancel() = 0;
    };
}
