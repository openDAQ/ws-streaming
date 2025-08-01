#pragma once

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/unit.hpp>

namespace wss
{
    /**
     * Semantically generates metadata describing a structure-valued signal. Applications can use
     * this class in conjunction with wss::metadata::struct_field() to describe a structure.
     */
    class struct_field_builder
    {
        public:

            /**
             * Constructs a builder object for a structure field with the specified name.
             *
             * @param name The name of the field.
             */
            struct_field_builder(
                const std::string& name);

            /**
             * Identifies this field as a one-dimensional array with the specified number of
             * elements.
             *
             * @param size The number of elements in the array.
             *
             * @return A reference to this object.
             */
            struct_field_builder& array(
                std::size_t size);

            /**
             * Sets the data type string of the field. The wss::data_types namespace contains
             * constants for the data types specified by the WebSocket Streaming Protocol
             * specification, but user-defined data types are also allowed.
             *
             * @param type The data type string of the field.
             *
             * @return A reference to this object.
             */
            struct_field_builder& data_type(
                const std::string& type);

            /**
             * Gets a JSON representation of this field.
             *
             * @return A JSON representation of this field.
             */
            const nlohmann::json& build() const noexcept;

        private:

            nlohmann::json _field;
    };
}
