#pragma once

#include <memory>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/transport/peer.hpp>

namespace wss::detail
{
    struct command_interface_client_factory
    {
        static
        std::unique_ptr<command_interface_client>
        create_client(
            const nlohmann::json& interfaces,
            const std::shared_ptr<transport::peer>& peer);
    };
}
