#pragma once

#include <cstddef>
#include <string>

namespace wss
{
    /**
     * Contains constants for signal data type strings defined by the WebSocket Streaming Protocol
     * specification. User-defined data type strings are also allowed.
     */
    namespace data_types
    {
        static constexpr const char *int8 = "int8";         /**< Signals are 8-bit signed integers. */
        static constexpr const char *int16 = "int16";       /**< Signals are 16-bit signed integers. */
        static constexpr const char *int32 = "int32";       /**< Signals are 32-bit signed integers. */
        static constexpr const char *int64 = "int64";       /**< Signals are 64-bit signed integers. */
        static constexpr const char *uint8 = "uint8";       /**< Signals are 8-bit unsigned integers. */
        static constexpr const char *uint16 = "uint16";     /**< Signals are 16-bit unsigned integers. */
        static constexpr const char *uint32 = "uint32";     /**< Signals are 32-bit unsigned integers. */
        static constexpr const char *uint64 = "uint64";     /**< Signals are 64-bit unsigned integers. */
        static constexpr const char *real32 = "real32";     /**< Signals are 32-bit floating-point numbers. */
        static constexpr const char *real64 = "real64";     /**< Signals are 64-bit floating-point numbers. */
    };
}
