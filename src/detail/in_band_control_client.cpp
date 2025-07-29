#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/transport/peer.hpp>
#include <ws-streaming/detail/in_band_control_client.hpp>

wss::detail::in_band_control_client::in_band_control_client(
        std::shared_ptr<transport::peer> peer)
    : _peer(peer)
{
}

void wss::detail::in_band_control_client::async_request(
    const std::string& method,
    const nlohmann::json& params,
    std::function<
        void(
            const boost::system::error_code& ec,
            const nlohmann::json& response)
    > handler)
{
    unsigned id = _next_id++;

    _peer->send_metadata(
        0,
        "controlRequest",
        {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "method", method },
            { "params", params },
        });

    _requests.emplace(id, std::move(handler));
}

void wss::detail::in_band_control_client::cancel()
{
    decltype(_requests) old_requests;

    std::swap(_requests, old_requests);

    for (const auto& entry : old_requests)
        entry.second(boost::asio::error::operation_aborted, nullptr);
}

void wss::detail::in_band_control_client::handle_response(const nlohmann::json& params)
{
    if (!params.contains("id")
            || !params["id"].is_number_integer())
        return;

    unsigned id = params["id"];

    auto it = _requests.find(id);
    if (it == _requests.end())
        return;

    decltype(_requests)::mapped_type handler = std::move(it->second);
    _requests.erase(it);

    handler({}, params.value<nlohmann::json>("result", nullptr));
}
