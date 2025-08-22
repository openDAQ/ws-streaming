#include <cstdint>
#include <memory>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/command_interface_client.hpp>
#include <ws-streaming/detail/command_interface_client_factory.hpp>
#include <ws-streaming/detail/http_command_interface_client.hpp>
#include <ws-streaming/detail/in_band_command_interface_client.hpp>
#include <ws-streaming/detail/peer.hpp>

std::unique_ptr<wss::detail::command_interface_client>
wss::detail::command_interface_client_factory::create_client(
    const nlohmann::json& interfaces,
    const std::shared_ptr<peer>& peer)
{
    if (!interfaces.is_object())
        return nullptr;

    // Prefer the in-band command interface ("jsonrpc") if it's supported.
    if (interfaces.contains("jsonrpc"))
        return std::make_unique<in_band_command_interface_client>(peer);

    if (interfaces.contains("jsonrpc-http")
        && interfaces["jsonrpc-http"].is_object()
        && interfaces["jsonrpc-http"].contains("httpMethod")
        && interfaces["jsonrpc-http"]["httpMethod"].is_string()
        && interfaces["jsonrpc-http"].contains("httpPath")
        && interfaces["jsonrpc-http"]["httpPath"].is_string()
        && interfaces["jsonrpc-http"].contains("httpVersion")
        && interfaces["jsonrpc-http"]["httpVersion"].is_string()
        && interfaces["jsonrpc-http"].contains("port")
        && (interfaces["jsonrpc-http"]["port"].is_string() || interfaces["jsonrpc-http"]["port"].is_number_integer()))
    {
        std::string port;
        if (interfaces["jsonrpc-http"]["port"].is_number_integer())
            port = std::to_string(interfaces["jsonrpc-http"]["port"].is_number_integer());
        else
            port = interfaces["jsonrpc-http"]["port"];

        return std::make_unique<http_command_interface_client>(
            peer->socket().get_executor(),
            peer->socket().remote_endpoint().address().to_string(),
            port,
            interfaces["jsonrpc-http"]["httpMethod"],
            interfaces["jsonrpc-http"]["httpPath"],
            interfaces["jsonrpc-http"]["httpVersion"]);
    }

    // There are no available/supported command interfaces.
    return nullptr;
}
