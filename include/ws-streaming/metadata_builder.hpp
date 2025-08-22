#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/struct_field_builder.hpp>
#include <ws-streaming/unit.hpp>

namespace wss
{
    /**
     * An empty class tag type used to disambiguate the metadata_builder constructor overload
     * which takes an nlohmann::json reference.
     */
    struct from_json_t { explicit from_json_t() = default; };

    /**
     * An instance of from_json_t used to disambiguate the metadata_builder constructor overload
     * which takes an nlohmann::json reference.
     */
    constexpr inline from_json_t from_json { };

    /**
     * Semantically generates metadata describing a signal. Applications can use this class to
     * build a wss::metadata object for a signal sourced by the application.
     */
    class metadata_builder
    {
        public:

            /**
             * Constructs a builder object for a signal with the specified name and global
             * identifier.
             *
             * @param name The name of the signal. This string need not be unique.
             */
            metadata_builder(
                const std::string& name);

            /**
             * Constructs a builder object that adopts existing JSON metadata as an initial value.
             *
             * @param metadata The existing metadata to adopt as an initial value.
             */
            metadata_builder(from_json_t,
                const nlohmann::json& metadata);

            /**
             * Gives the signal a constant rule.
             *
             * @return A reference to this object.
             */
            metadata_builder& constant_rule();

            /**
             * Sets the data type string of the signal. The wss::data_types namespace contains
             * constants for the data types specified by the WebSocket Streaming Protocol
             * specification, but user-defined data types are also allowed.
             *
             * @param type The data type string of the signal.
             *
             * @return A reference to this object.
             */
            metadata_builder& data_type(
                const std::string& type);

            /**
             * Sets the endianness of the signal. The wss::endianness namespace contains constants
             * for the data types specified by the WebSocket Streaming Protocol specification.
             * User-defined data types are allowed, but probably do no make sense.
             *
             * @param endian The endianness of the signal.
             *
             * @return A reference to this object.
             */
            metadata_builder& endian(
                const std::string& endian);

            /**
             * Gives the signal a linear rule with the specified starting point and delta.
             *
             * @param start The initial value of the signal in ticks.
             * @param delta The number of ticks by which the signal's value increments for each
             *     sample.
             *
             * @return A reference to this object.
             */
            metadata_builder& linear_rule(
                std::int64_t start,
                std::int64_t delta);

            /**
             * Sets the origin string of this signal. For time signals, this value is an ISO 8601
             * date/time string specifying the calendar time represented by zero ticks.
             *
             * @param origin The origin string of this signal.
             *
             * @return A reference to this object.
             */
            metadata_builder& origin(
                const std::string& origin);

            /**
             * Sets the value range of this signal.
             *
             * @param low The minimum expected value of the signal.
             * @param high The maximum expected value of the signal.
             *
             * @return A reference to this object.
             */
            metadata_builder& range(
                double low,
                double high);

            /**
             * Adds a field to the definition of the signal's struct data type.
             *
             * @param field A builder object used to generate the field description.
             *
             * @return A reference to this object.
             */
            metadata_builder& struct_field(
                const struct_field_builder& field);

            /**
             * Assigns an associated domain signal.
             *
             * @param id The global identifier of the associated domain signal. A local_signal
             *     with this identifier should be registered with every streaming endpoint with
             *     which this signal is registered.
             *
             * @return A reference to this object.
             */
            metadata_builder& table(
                const std::string& id);

            /**
             * For direct TCP protocol devices, sets the signal rate data to the specified JSON.
             *
             * @param signal_rate The direct TCP protocol device's `signalRate` specification.
             *     This value should be a JSON object containing keys `delta` and `samples`.
             */
            metadata_builder& tcp_signal_rate(
                const nlohmann::json& signal_rate);

            /**
             * Sets the magnitude of a single tick for linear-rule signals. This value specifies
             * how much of the signal's unit value is represented by a single tick. Tick
             * resolutions are specified as ratios to allow exact representation of any rational
             * value.
             *
             * @param numerator The numerator of the tick resolution ratio.
             * @param denominator The denominator of the tick resolution ratio.
             *
             * @return A reference to this object.
             */
            metadata_builder& tick_resolution(
                std::uint64_t numerator,
                std::uint64_t denominator);

            /**
             * Specifies the unit of measurement of the signal.
             *
             * @param unit A unit object.
             *
             * @return A reference to this object.
             */
            metadata_builder& unit(const unit& unit);

            /**
             * Specifies the unit of measurement of the signal.
             *
             * @param id The numeric identifier of the unit.
             * @param name The name of the unit, such as "seconds".
             * @param quantity The physical quantity represented by the unit, such as "time". The
             *     wss::quantities namespace contains constants for the quantities specified by
             *     the WebSocket Streaming Protocol specification, but user-defined quantities are
             *     also allowed.
             * @param symbol The symbol of the unit, which is a string that can be appended to a
             *     numeric value, such as "s".
             *
             * @return A reference to this object.
             */
            metadata_builder& unit(
                int id,
                const std::string& name,
                const std::string& quantity,
                const std::string& symbol);

            /**
             * Gets a JSON representation of this metadata.
             *
             * @return A JSON representation of this metadata.
             */
            const nlohmann::json& build() const noexcept;

        private:

            nlohmann::json _metadata;
    };
}
