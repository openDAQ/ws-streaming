#pragma once

#include <array>
#include <cstdint>

#pragma pack(push, 1)
struct can_message
{
    std::uint32_t message_id;
    std::uint8_t payload_length;
    std::array<std::uint8_t, 64> payload;
};
#pragma pack(pop)
