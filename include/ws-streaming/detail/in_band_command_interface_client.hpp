#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/transport/peer.hpp>

namespace wss::detail
{
    class in_band_command_interface_client
        : public command_interface_client
    {
        public:

            in_band_command_interface_client(
                std::shared_ptr<transport::peer> peer);

            void async_request(
                const std::string& method,
                const nlohmann::json& params,
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const nlohmann::json& response)
                > handler) override;

            void cancel() override;

            void handle_response(const nlohmann::json& params);

        private:

            std::shared_ptr<transport::peer> _peer;
            unsigned _next_id = 1;

            std::map<unsigned, std::function<
                void(
                    const boost::system::error_code& ec,
                    const nlohmann::json& response)
            >> _requests;
    };
}
