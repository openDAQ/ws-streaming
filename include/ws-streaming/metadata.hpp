#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace wss
{
    /**
     * Contains metadata that describes a signal. The description specifies the type and format of
     * data published by the signal, as well as information about how to interpret that data, such
     * as its range, units of measurement, and domain.
     *
     * Metadata is stored and transmitted in JSON format. This class is a wrapper around a JSON
     * object. The metadata_builder class can be used to generate metadata, and provides member
     * functions to semantically generate a signal description. The accessor functions in this
     * class inspect and decode the wrapped JSON.
     */
    class metadata
    {
        public:

            /**
             * Constructs an empty metadata object. Such metadata is not valid for a signal.
             */
            metadata();

            /**
             * Constructs a metadata object from the specified JSON.
             *
             * @param json A reference to a JSON object containing the signal metadata. This JSON
             *     object should come from a metadata_builder object, or from the JSON transmitted
             *     by a remote peer.
             */
            metadata(const nlohmann::json& json);

            /**
             * Gets the data type string of the signal. The wss::data_types namespace contains
             * constants for the data types specified by the WebSocket Streaming Protocol
             * specification, but user-defined data types are also allowed.
             *
             * @return The data type string of the signal, or an empty string if the metadata does
             *     not specify a data type.
             */
            std::string data_type() const;

            /**
             * Tests whether the signal has a linear rule.
             *
             * @return True if the signal has a linear rule.
             */
            bool is_linear_rule() const;

            /**
             * Gets the signal's linear-rule start and delta parameters.
             *
             * @return A pair containing the signal's linear-rule start and delta parameters, in
             *     that order. If the signal does not have a linear rule, or one of the parameters
             *     is missing in the signal's metadata, zeroes are returned instead.
             */
            std::pair<std::int64_t, std::int64_t> linear_start_delta() const;

            /**
             * Calculates the size of a sample, if fixed and if the data type is one recognized by
             * the WebSocket Streaming Protocol specification.
             *
             * @return The size of a single sample of this signal, in bytes. The return value is
             *     zero if the signal is not explicit-rule, or if the data type is unknown or
             *     user-defined.
             */
            std::size_t sample_size() const;

            /**
             * Gets the global identifier of the associated domain signal, if any.
             *
             * @return The global identifier of the associated domain signal, or an empty string
             *     if no associated domain signal is specified in the signal's metadata.
             */
            std::string table_id() const;

            /**
             * Gets the "value index," or the sample count at which the last-transmitted
             * linear-rule value applies.
             *
             * @return The metadata's value index, or std::nullopt if not specified.
             */
            std::optional<std::int64_t> value_index() const;

            /**
             * Gets a reference to the underlying JSON object containing the signal metadata.
             *
             * @return A reference to the underlying JSON object containing the signal metadata.
             */
            const nlohmann::json& json() const noexcept;

        private:

            nlohmann::json _json;
    };
}
