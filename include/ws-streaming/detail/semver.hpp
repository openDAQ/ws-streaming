#pragma once

#include <optional>
#include <string>

namespace wss::detail
{
    class semver
    {
        public:

            static std::optional<semver> try_parse(const std::string& str);

            semver() noexcept
                : _major(0)
                , _minor(0)
                , _revision(0)
            {
            }

            semver(unsigned major, unsigned minor, unsigned revision) noexcept
                : _major(major)
                , _minor(minor)
                , _revision(revision)
            {
            }

            unsigned major() const noexcept
            {
                return _major;
            }

            unsigned minor() const noexcept
            {
                return _minor;
            }

            unsigned revision() const noexcept
            {
                return _revision;
            }

            bool operator==(const semver& rhs) const noexcept
            {
                return _major == rhs._major
                    && _minor == rhs._minor
                    && _revision == rhs._revision;
            }

            bool operator!=(const semver& rhs) const noexcept
            {
                return !(*this == rhs);
            }

            bool operator<(const semver& rhs) const noexcept
            {
                if (_major < rhs._major)
                    return true;
                else if (_major > rhs._major)
                    return false;
                else if (_minor < rhs._minor)
                    return true;
                else if (_minor > rhs._minor)
                    return false;
                else return _revision < rhs._revision;
            }

            bool operator>(const semver& rhs) const noexcept
            {
                return *this != rhs && !(*this < rhs);
            }

            bool operator>=(const semver& rhs) const noexcept
            {
                return *this == rhs || *this > rhs;
            }

            bool operator<=(const semver& rhs) const noexcept
            {
                return *this == rhs || *this < rhs;
            }

            private:

            unsigned _major;
            unsigned _minor;
            unsigned _revision;
    };
}