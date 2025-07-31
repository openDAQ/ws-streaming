#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/remote_signal_container.hpp>

std::pair<bool, wss::detail::remote_signal_container::remote_signal_entry&>
wss::detail::remote_signal_container::add_remote_signal(const std::string& id)
{
    auto it = _signals_by_id.find(id);
    if (it != _signals_by_id.end())
        return std::make_pair(true, std::ref(it->second));

    auto [jt, emplaced] = _signals_by_id.emplace(id, id);
    return std::make_pair(true, std::ref(jt->second));
}

wss::detail::remote_signal_container::remote_signal_entry *
wss::detail::remote_signal_container::find_remote_signal(const std::string& id)
{
    auto it = _signals_by_id.find(id);
    if (it == _signals_by_id.end())
        return nullptr;

    return &it->second;
}

wss::detail::remote_signal_container::remote_signal_entry *
wss::detail::remote_signal_container::find_remote_signal(unsigned signo)
{
    auto it = _signals_by_signo.find(signo);
    if (it == _signals_by_signo.end())
        return nullptr;

    return it->second;
}
