#pragma once

#include <cstddef>
#include <string>

namespace wss
{
    /**
     * Contains constants for rule type strings defined by the WebSocket Streaming Protocol
     * specification.
     */
    namespace rule_types
    {
        static constexpr const char *explicit_rule = "explicit";    /**< Explicit data is given for each sample. */
        static constexpr const char *constant_rule = "constant";    /**< Data is linearly interpolated for each sample. */
        static constexpr const char *linear_rule = "linear";        /**< Data is constant for every sample. */
    };
}
