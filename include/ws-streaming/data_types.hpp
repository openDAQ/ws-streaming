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
        static constexpr const char *int8_t = "int8";       /**< Signals are 8-bit signed integers. */
        static constexpr const char *int16_t = "int16";     /**< Signals are 16-bit signed integers. */
        static constexpr const char *int32_t = "int32";     /**< Signals are 32-bit signed integers. */
        static constexpr const char *int64_t = "int64";     /**< Signals are 64-bit signed integers. */
        static constexpr const char *uint8_t = "uint8";     /**< Signals are 8-bit unsigned integers. */
        static constexpr const char *uint16_t = "uint16";   /**< Signals are 16-bit unsigned integers. */
        static constexpr const char *uint32_t = "uint32";   /**< Signals are 32-bit unsigned integers. */
        static constexpr const char *uint64_t = "uint64";   /**< Signals are 64-bit unsigned integers. */
        static constexpr const char *real32_t = "real32";   /**< Signals are 32-bit floating-point numbers. */
        static constexpr const char *real64_t = "real64";   /**< Signals are 64-bit floating-point numbers. */
        static constexpr const char *struct_t = "struct";   /**< Signals are structure-valued. */
    };
}
