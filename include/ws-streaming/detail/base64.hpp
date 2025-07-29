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
     * @tparam ConstIterator A constant iterator type for values convertible to std::uint8_t.
     *
     * @param begin An iterator to the first byte to encode.
     * @param end An iterator past the last byte to encode.
     *
     * @return A Base64 string representation of the specified bytes.
     */
    template <typename ConstIterator>
    std::string base64(ConstIterator begin, ConstIterator end)
    {
        using namespace boost::archive::iterators;
        using It = base64_from_binary<transform_width<ConstIterator, 6, 8>>;
        auto tmp = std::string(It(begin), It(end));
        return tmp.append((3 - (end - begin) % 3) % 3, '=');
    }

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
    std::string base64(const Container& bytes)
    {
        return base64(std::begin(bytes), std::end(bytes));
    }
}
