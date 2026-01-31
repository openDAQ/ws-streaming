#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/remote_signal_container.hpp>

std::pair<bool, wss::detail::remote_signal_container::remote_signal_entry&>
wss::detail::remote_signal_container::add_remote_signal(
    const std::string& id,
    bool hidden)
{
    auto it = _signals_by_id.find(id);
    if (it != _signals_by_id.end())
        return std::make_pair(true, std::ref(it->second));

    auto [jt, emplaced] = _signals_by_id.emplace(
        std::piecewise_construct,
        std::make_tuple(id),
        std::make_tuple(id, hidden));
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
wss::detail::remote_signal_container::find_table(const std::string& table_id)
{
    auto it = std::find_if(
        _signals_by_id.begin(),
        _signals_by_id.end(),
        [table_id](const decltype(_signals_by_id)::value_type& entry)
        {
            auto table = entry.second.signal->table();
            return table && !table->id().empty() && table->id() == table_id;
        });

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

const wss::detail::remote_signal_container::remote_signal_entry *
wss::detail::remote_signal_container::find_remote_signal(const std::string& id) const
{
    auto it = _signals_by_id.find(id);
    if (it == _signals_by_id.end())
        return nullptr;

    return &it->second;
}

const wss::detail::remote_signal_container::remote_signal_entry *
wss::detail::remote_signal_container::find_table(const std::string& table_id) const
{
    auto it = std::find_if(
        _signals_by_id.begin(),
        _signals_by_id.end(),
        [table_id](const decltype(_signals_by_id)::value_type& entry)
        {
            auto table = entry.second.signal->table();
            return table && !table->id().empty() && table->id() == table_id;
        });

    if (it == _signals_by_id.end())
        return nullptr;

    return &it->second;
}

const wss::detail::remote_signal_container::remote_signal_entry *
wss::detail::remote_signal_container::find_remote_signal(unsigned signo) const
{
    auto it = _signals_by_signo.find(signo);
    if (it == _signals_by_signo.end())
        return nullptr;

    return it->second;
}
