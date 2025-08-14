#include <algorithm>
#include <functional>
#include <string>
#include <utility>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/detail/local_signal_container.hpp>
#include <ws-streaming/detail/registered_local_signal.hpp>

std::pair<std::reference_wrapper<wss::detail::registered_local_signal>, bool>
wss::detail::local_signal_container::add_local_signal(local_signal& signal)
{
    auto it = std::find_if(
        _signals.begin(),
        _signals.end(),
        [signal = &signal](const decltype(_signals)::value_type& entry)
        {
            return &entry.second.signal == signal;
        });

    if (it != _signals.end())
        return std::make_pair(
            std::ref(it->second),
            false);

    unsigned signo = _next_signo++;
    auto result = _signals.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(signo),
        std::forward_as_tuple(signal, signo));

    return std::make_pair(
        std::ref(result.first->second),
        true);
}

unsigned wss::detail::local_signal_container::remove_local_signal(local_signal& signal)
{
    auto it = std::find_if(
        _signals.begin(),
        _signals.end(),
        [signal = &signal](const decltype(_signals)::value_type& entry)
        {
            return &entry.second.signal == signal;
        });

    if (it == _signals.end())
        return 0;

    unsigned signo = it->first;
    _signals.erase(it);
    return signo;
}

void wss::detail::local_signal_container::clear_local_signals()
{
    _signals.clear();
}

wss::detail::registered_local_signal *
wss::detail::local_signal_container::find_local_signal(const std::string& id)
{
    auto it = std::find_if(
        _signals.begin(),
        _signals.end(),
        [&](const decltype(_signals)::value_type& entry)
        {
            return entry.second.signal.id() == id;
        });

    if (it == _signals.end())
        return nullptr;

    return &it->second;
}

wss::detail::registered_local_signal *
wss::detail::local_signal_container::find_local_signal(unsigned signo)
{
    auto it = _signals.find(signo);

    if (it == _signals.end())
        return nullptr;

    return &it->second;
}
