#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <ws-streaming/struct_field_dimension.hpp>

namespace wss
{
    /**
     * Contains metadata that describes a structure field.
     */
    class struct_field
    {
        public:

            /**
             * Constructs an empty metadata object. Such metadata is not valid for a field.
             */
            struct_field();

            /**
             * Constructs a metadata object from the specified JSON.
             *
             * @param json A reference to a JSON object containing the field metadata. This JSON
             *     object should come from the JSON transmitted by a remote peer.
             */
            struct_field(const nlohmann::json& json);

            /**
             * Gets the data type string of the field. The wss::data_types namespace contains
             * constants for the data types specified by the WebSocket Streaming Protocol
             * specification, but user-defined data types are also allowed.
             *
             * @return The data type string of the field, or data_types::unknown_t ("unknown") if
             *     the metadata does not specify a data type.
             */
            std::string data_type() const;

            /**
             * Populates and returns a container with the set of dimensions defined for this
             * field.
             *
             * @tparam Container A container type which can be default-constructed and which has
             *     an emplace_back() member function.
             *
             * @return The set of dimensions defined, or an empty (default-constructed) collection
             *     if there are no defined dimensions in this metadata.
             */
            template <typename Container = std::vector<struct_field_dimension>>
            Container dimensions() const
            {
                Container container;

                if (_json.contains("dimensions")
                        && _json["dimensions"].is_array())
                    for (const auto& dim : _json["dimensions"])
                        container.emplace_back(dim);

                return container;
            }

            /**
             * Gets the name of the field.
             *
             * @return The name of the field, or an empty string if the metadata does not specify
             *     a field name.
             */
            std::string name() const;

            /**
             * Gets the rule type string of this field.
             *
             * @return The rule type string of this field, or "explicit" if not otherwise set.
             */
            std::string rule() const;

        private:

            nlohmann::json _json;
    };
}
