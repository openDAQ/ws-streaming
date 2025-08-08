#pragma once

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/dimension_builder.hpp>
#include <ws-streaming/unit.hpp>

namespace wss
{
    /**
     * Semantically generates metadata describing a dimension.
     */
    class dimension_builder
    {
        public:

            /**
             * Constructs a builder object for a dimension with the specified name.
             *
             * @param name The name of the dimension.
             */
            dimension_builder(
                const std::string& name);

            /**
             * Gives the dimension a linear rule with the specified starting point, delta and
             * size.
             *
             * @param start The initial value of the dimension.
             * @param delta The amount by which the dimension's value increases for each element.
             * @param size The total number of elements in the dimension.
             *
             * @return A reference to this object.
             */
            dimension_builder& linear_rule(
                std::int64_t start,
                std::int64_t delta,
                std::uint64_t size);

            /**
             * Gets a JSON representation of this dimension.
             *
             * @return A JSON representation of this dimension.
             */
            const nlohmann::json& build() const noexcept;

        private:

            nlohmann::json _dimension;
    };
}
