#pragma once

#include <type_traits>

#if WS_STREAMING_ENABLE_TLS
#include <boost/asio/ssl/stream.hpp>
#endif

namespace wss::detail
{
    template <typename T>
    struct is_ssl_stream : std::false_type {};

#if WS_STREAMING_ENABLE_TLS

    template <typename NextLayer>
    struct is_ssl_stream<boost::asio::ssl::stream<NextLayer>> : std::true_type {};

#endif

    template <typename T>
    inline constexpr bool is_ssl_stream_v = is_ssl_stream<T>::value;
}
