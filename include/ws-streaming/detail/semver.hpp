#pragma once

#include <optional>
#include <string>

namespace wss::detail
{
    /**
     * Represents a <a href="https://semver.org/">semver-style</a> version number, consisting of
     * major, minor and revision integers.
     */
    class semver
    {
        public:

            /**
             * Parses a version string.
             *
             * @param str The string to parse. The string should be of the form "a.b.c" where a, b
             *     and c are nonnegative integers.
             *
             * @return A semver object if the string was successfully parsed, or std::nullopt.
             */
            static std::optional<semver> try_parse(const std::string& str);

            /**
             * Constructs a version object for the version 0.0.0.
             */
            semver() noexcept
                : _major(0)
                , _minor(0)
                , _revision(0)
            {
            }

            /**
             * Constructs a version object with the specified major, minor and revision
             * components.
             *
             * @param major The major version number.
             * @param minor The minor version number.
             * @param revision The revision number.
             */
            semver(unsigned major, unsigned minor, unsigned revision) noexcept
                : _major(major)
                , _minor(minor)
                , _revision(revision)
            {
            }

            /**
             * Gets the major version number.
             *
             * @return The major version number.
             */
            unsigned major() const noexcept
            {
                return _major;
            }

            /**
             * Gets the minor version number.
             *
             * @return The minor version number.
             */
            unsigned minor() const noexcept
            {
                return _minor;
            }

            /**
             * Gets the revision number.
             *
             * @return The revision number.
             */
            unsigned revision() const noexcept
            {
                return _revision;
            }

            /**
             * Tests if two version objects represent the same version.
             *
             * @param rhs The other version object to compare.
             *
             * @return True if the two version objects have the same major, minor and revision
             *     components.
             */
            bool operator==(const semver& rhs) const noexcept
            {
                return _major == rhs._major
                    && _minor == rhs._minor
                    && _revision == rhs._revision;
            }

            /**
             * Tests if two version objects represent different versions.
             *
             * @param rhs The other version object to compare.
             *
             * @return True if the two version objects do not have the same major, minor and
             *     revision components.
             */
            bool operator!=(const semver& rhs) const noexcept
            {
                return !(*this == rhs);
            }

            /**
             * Tests if the version represented by this object is less than that represented by
             * another object.
             *
             * @param rhs The other version object to compare.
             *
             * @return True if the this version object has a lower major version number, or the
             *     same version number and a lower minor version number, or the same major and
             *     minor version numbers and a lower revision number than that represented by
             *     @p rhs.
             */
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

            /**
             * Tests if the version represented by this object is less than or equal to that
             * represented by another object.
             *
             * @param rhs The other version object to compare.
             *
             * @return Equivalent to `*this < rhs || *this == rhs`.
             */
            bool operator<=(const semver& rhs) const noexcept
            {
                return *this == rhs || *this < rhs;
            }

            /**
             * Tests if the version represented by this object is greater than that represented by
             * another object.
             *
             * @param rhs The other version object to compare.
             *
             * @return True if the this version object has a higher major version number, or the
             *     same version number and a higher minor version number, or the same major and
             *     minor version numbers and a higher revision number than that represented by
             *     @p rhs.
             */
            bool operator>(const semver& rhs) const noexcept
            {
                return *this != rhs && !(*this < rhs);
            }

            /**
             * Tests if the version represented by this object is less than or equal to that
             * represented by another object.
             *
             * @param rhs The other version object to compare.
             *
             * @return Equivalent to `*this > rhs || *this == rhs`.
             */
            bool operator>=(const semver& rhs) const noexcept
            {
                return *this == rhs || *this > rhs;
            }

        private:

            unsigned _major;
            unsigned _minor;
            unsigned _revision;
    };
}
