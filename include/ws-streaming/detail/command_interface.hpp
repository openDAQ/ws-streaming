#pragma once

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace wss::detail
{
    class command_interface
    {
        public:

            virtual ~command_interface()
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
