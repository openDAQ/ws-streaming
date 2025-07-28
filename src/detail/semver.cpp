#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

#include <ws-streaming/detail/semver.hpp>

std::optional<wss::detail::semver>
wss::detail::semver::try_parse(const std::string& str)
{
    std::size_t a = str.find('.');
    if (a >= str.length())
        return std::nullopt;

    std::cout << "aa" << std::endl;
    std::size_t b = str.find('.', a + 1);
    if (b >= str.length())
        return std::nullopt;
    std::cout << "bb" << std::endl;

    std::string major_str = str.substr(0, a);
    std::string minor_str = str.substr(a + 1, b - a - 1);
    std::string revision_str = str.substr(b + 1);

    char *end;

    errno = 0;
    unsigned long major = std::strtoul(
        major_str.c_str(),
        &end,
        10);
    if (errno || *end != '\0' || major > std::numeric_limits<unsigned>::max())
        return std::nullopt;
    std::cout << "cc '" << minor_str.c_str() << '\'' << std::endl;

    errno = 0;
    unsigned long minor = std::strtoul(
        minor_str.c_str(),
        &end,
        10);
    if (errno || *end != '\0' || minor > std::numeric_limits<unsigned>::max())
        return std::nullopt;
    std::cout << "dd '" << revision_str.c_str() << '\'' << std::endl;

    errno = 0;
    unsigned long revision = std::strtoul(
        revision_str.c_str(),
        &end,
        10);
    if (errno || *end != '\0' || revision > std::numeric_limits<unsigned>::max())
        return std::nullopt;
    std::cout << "ee" << std::endl;

    return semver(major, minor, revision);
}
