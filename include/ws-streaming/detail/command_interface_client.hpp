#pragma once

#include <functional>
#include <string>

#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

namespace wss::detail
{
    class command_interface_client
    {
        public:

            virtual ~command_interface_client()
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
