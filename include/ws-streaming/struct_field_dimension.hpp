#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include <nlohmann/json.hpp>

namespace wss
{
    /**
     * Contains metadata that describes a dimension of a structure field dimension.
     */
    class struct_field_dimension
    {
        public:

            /**
             * Constructs an empty metadata object. Such metadata is not valid for a field
             * dimension.
             */
            struct_field_dimension();

            /**
             * Constructs a metadata object from the specified JSON.
             *
             * @param json A reference to a JSON object containing the dimension metadata. This
             *     JSON object should come from the JSON transmitted by a remote peer.
             */
            struct_field_dimension(const nlohmann::json& json);

            /**
             * Gets the signal's linear-rule start and delta parameters.
             *
             * @return A tuple containing the signal's linear-rule start, delta and size
             *     parameters, in that order. If the signal does not have a linear rule, or one of
             *     the parameters is missing in the signal's metadata, std::nullopt is returned in
             *     that position instead.
             */
            std::tuple<
                std::optional<std::int64_t>,
                std::optional<std::int64_t>,
                std::optional<std::int64_t>
            > linear_start_delta_size() const;

            /**
             * Gets the name of the dimension.
             *
             * @return The name of the dimension, or an empty string if the metadata does not specify
             *     a dimension name.
             */
            std::string name() const;

            /**
             * Gets the rule type string of this dimension.
             *
             * @return The rule type string of this dimension, or "explicit" if not otherwise set.
             */
            std::string rule() const;

        private:

            nlohmann::json _json;
    };
}
