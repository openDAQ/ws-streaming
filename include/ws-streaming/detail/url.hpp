#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wss::detail
{
    class url
    {
        public:

            url(std::string_view str);

            const std::string& host_address() const noexcept { return _host_address; }

            const std::string& path() const noexcept { return _path; }

            const std::optional<std::uint16_t>& port_number() const noexcept { return _port_number; }

            const std::string& scheme() const noexcept { return _scheme; }

        private:

            std::string _host_address;
            std::string _path;
            std::optional<std::uint16_t> _port_number;
            std::string _scheme;
    };
}
