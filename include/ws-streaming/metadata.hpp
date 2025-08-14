#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include <ws-streaming/unit.hpp>

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
             * The "UNIX Epoch" expressed as an ISO-8601 date/time string, suitable for use as an
             * origin() value.
             */
            static constexpr const char *unix_epoch = "1970-01-01T00:00:00.000Z";

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
             * Gets the signal's linear-rule start and delta parameters.
             *
             * @return A pair containing the signal's linear-rule start and delta parameters, in
             *     that order. If the signal does not have a linear rule, or one of the parameters
             *     is missing in the signal's metadata, std::nullopt is returned in that position
             *     instead.
             */
            std::pair<
                std::optional<std::int64_t>,
                std::optional<std::int64_t>
            > linear_start_delta() const;

            /**
             * Gets the name of the signal.
             *
             * @return The name of the signal, or an empty string if the metadata does not specify
             *     a signal name.
             */
            std::string name() const;

            /**
             * Gets the origin string of this signal. For time signals, this value is an ISO 8601
             * date/time string specifying the calendar time represented by zero ticks.
             *
             * @return The origin string of this signal, or an empty string if not set.
             */
            std::string origin() const;

            /**
             * Gets the value range of this signal.
             *
             * @return A pair consisting of the minimum and maximum expected values, respectively,
             *     of the signal; or std::nullopt if not set.
             */
            std::optional<std::pair<double, double>> range() const;

            /**
             * Gets the rule type string of this signal.
             *
             * @return The rule type string of this signal, or "explicit" if not otherwise set.
             */
            std::string rule() const;

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
             * Gets the magnitude of a single tick for linear-rule signals. This value specifies
             * how much of the signal's unit value is represented by a single tick. Tick
             * resolutions are specified as ratios to allow exact representation of any rational
             * value.
             *
             * @return A pair consisting of the numerator and denominator, respectively, of the
             *     tick resolution ratio; or std::nullopt if not set.
             */
            std::optional<std::pair<std::uint64_t, std::uint64_t>> tick_resolution() const;

            /**
             * Gets the unit of measurement of the signal.
             *
             * @return A unit object, or std::nullopt if not set.
             */
            std::optional<wss::unit> unit() const;

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
