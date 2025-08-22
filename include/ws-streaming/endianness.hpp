#pragma once

namespace wss
{
    /**
     * Contains constants for signal endianness strings defined by the WebSocket Streaming
     * Protocol specification. User-defined endianness strings are allowed, but probably do not
     * make sense.
     */
    namespace endianness
    {
        static constexpr const char *unknown = "unknown";   /**< Signal endianness is unknown. */
        static constexpr const char *big = "big";           /**< Signal is big-endian. */
        static constexpr const char *little = "little";     /**< Signal is little-endian. */
    };
}
