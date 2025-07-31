#pragma once

namespace wss
{
    /**
     * Contains constants for physical quantity strings, used in unit specifications, defined by
     * the WebSocket Streaming Protocol specification. User-defined quantity strings are also
     * allowed.
     */
    namespace quantities
    {
        /**
         * Represents measurements of time.
         */
        static constexpr const char *time = "time";

        /**
         * Represents measurements of voltage.
         */
        static constexpr const char *voltage = "voltage";
    }
}
