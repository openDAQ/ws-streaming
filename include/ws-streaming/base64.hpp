#pragma once

#include <iterator>
#include <string>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>

namespace wss::detail
{
    /**
     * Generates a Base64 string representation of a sequence of bytes.
     *
     * @tparam Container A type for which std::begin() and std::end() will return iterators to a
     *     sequence of bytes to encode.
     *
     * @param bytes The sequence of bytes to encode.
     *
     * @return A Base64 string representation of the specified bytes.
     */
    template <typename Container>
    static std::string base64(const Container& bytes)
    {
        auto begin = std::begin(bytes);
        auto end = std::end(bytes);
        using namespace boost::archive::iterators;
        using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
        auto tmp = std::string(It(begin), It(end));
        return tmp.append((3 - (end - begin) % 3) % 3, '=');
    }
}
