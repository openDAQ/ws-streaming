#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/remote_signal.hpp>

const std::string& wss::remote_signal::id() const noexcept
{
    return _id;
}

bool wss::remote_signal::is_subscribed() const noexcept
{
    return _is_subscribed;
}

unsigned wss::remote_signal::signo() const noexcept
{
    return _signo;
}

const wss::metadata& wss::remote_signal::metadata() const noexcept
{
    return _metadata;
}

wss::remote_signal::remote_signal(const std::string& id)
    : _id(id)
{
}
