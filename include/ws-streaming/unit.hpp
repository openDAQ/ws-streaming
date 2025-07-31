#pragma once

#include <string>

namespace wss
{
    /**
     * Represents a unit of measurement. Unit specifications appear in WebSocket Streaming
     * Protocol signal metadata, and consist of a unique numeric identifier, name, quantity and
     * symbol. This class provides static constants for the units defined by the WebSocket
     * Streaming Protocol specification, but also allowed for user-defined units.
     */
    class unit
    {
        public:

            /**
             * A unit of "seconds", measuring "time" with a symbol of "s".
             */
            static unit seconds;

            /**
             * A unit of "volts", measuring "voltage" with a symbol of "V".
             */
            static unit volts;

        public:

            /**
             * Constructs a user-defined unit.
             *
             * @param id The numeric identifier of the unit.
             * @param name The name of the unit, such as "seconds".
             * @param quantity The physical quantity represented by the unit, such as "time". The
             *     wss::quantities namespace contains constants for the quantities specified by
             *     the WebSocket Streaming Protocol specification, but user-defined quantities are
             *     also allowed.
             * @param symbol The symbol of the unit, which is a string that can be appended to a
             *     numeric value, such as "s".
             */
            unit(
                int id,
                const std::string& name,
                const std::string& quantity,
                const std::string& symbol);

            /**
             * Gets the numeric identifier of the unit.
             *
             * @return The numeric identifier of the unit.
             */
            int id() const noexcept;

            /**
             * Gets the name of the unit, such as "seconds".
             *
             * @return The name of the unit, such as "seconds".
             */
            const std::string& name() const noexcept;

            /**
             * Gets the physical quantity represented by the unit, such as "time". The
             * wss::quantities namespace contains constants for the quantities specified by the
             * WebSocket Streaming Protocol specification, but user-defined quantities are also
             * allowed.
             *
             * @return The physical quantity represented by the unit, such as "time".
             */
            const std::string& quantity() const noexcept;

            /**
             * Gets the symbol of the unit, which is a string that can be appended to a numeric
             * value, such as "s".
             *
             * @return The symbol of the unit, which is a string that can be appended to a
             *     numeric value, such as "s".
             */
            const std::string& symbol() const noexcept;

        private:

            int _id;
            std::string _name;
            std::string _quantity;
            std::string _symbol;
    };
}
