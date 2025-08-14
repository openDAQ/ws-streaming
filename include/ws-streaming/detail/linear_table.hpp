#pragma once

#include <cstdint>

#include <ws-streaming/metadata.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>

namespace wss::detail
{
    class linear_table
    {
        public:

            linear_table(const metadata& metadata);

            void update(const metadata& metadata);

            void update(const streaming_protocol::linear_payload& payload);

            std::int64_t driven_value() const noexcept;

            std::int64_t value_at(std::int64_t index) const noexcept;

            void set(std::int64_t index, std::int64_t value) noexcept;

            void drive_to(std::int64_t index) noexcept;

            std::int64_t driven_index() const noexcept;

        private:

            std::int64_t _index = 0;
            std::int64_t _value = 0;
            std::int64_t _delta = 0;
            std::int64_t _driven_index = 0;
    };
}
