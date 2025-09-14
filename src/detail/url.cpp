#include <cstdlib>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

#include <ws-streaming/detail/url.hpp>

wss::detail::url::url(std::string_view str)
{
    std::regex re{R"(^([^:]+)://([^\[\]:/]+|\[[^\[\]/]+\])(?::(\d+))?(/.*)?)"};
    std::smatch matches;
    std::string as_str{str};

    if (!std::regex_search(as_str, matches, re))
        throw std::invalid_argument("invalid URL");

    _scheme = matches[1];
    _host_address = matches[2];
    _path = matches[4];

    if (!matches[3].str().empty())
        _port_number = std::atoi(matches[3].str().c_str());
}
