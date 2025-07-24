#include <cstddef>
#include <cstdint>
#include <cstring>

#include <ws-streaming/websocket_protocol.hpp>

wss::websocket_protocol::decoded_header
wss::websocket_protocol::decode_header(const std::uint8_t *data, std::size_t size) noexcept
{
    decoded_header header { };
    const std::uint8_t *data_begin = data;

    if (size < 2)
        return header;

    header.opcode = data[0] & 0xF;
    header.flags = data[0] & 0xF0;
    header.is_masked = 0 != (data[1] & 0x80);
    header.payload_size = data[1] & 0x7F;

    data += 2;
    size -= 2;

    if (header.payload_size == 126)
    {
        if (size < sizeof(std::uint16_t))
            return header;

        header.payload_size =
            (static_cast<std::uint16_t>(data[0]) << 8) |
            data[1];

        data += sizeof(std::uint16_t);
        size -= sizeof(std::uint16_t);
    }

    else if (header.payload_size == 127)
    {
        if (size < sizeof(std::uint64_t))
            return header;

        header.payload_size =
            (static_cast<std::uint64_t>(data[0]) << 56) |
            (static_cast<std::uint64_t>(data[1]) << 48) |
            (static_cast<std::uint64_t>(data[2]) << 40) |
            (static_cast<std::uint64_t>(data[3]) << 32) |
            (static_cast<std::uint64_t>(data[4]) << 24) |
            (static_cast<std::uint64_t>(data[5]) << 16) |
            (static_cast<std::uint64_t>(data[6]) << 8) |
            data[7];

        data += sizeof(std::uint64_t);
        size -= sizeof(std::uint64_t);
    }

    if (header.is_masked)
    {
        if (size < header.masking_key.size())
            return header;

        std::memcpy(
            header.masking_key.data(),
            data,
            header.masking_key.size());

        data += header.masking_key.size();
        size -= header.masking_key.size();
    }

    if (size >= header.payload_size)
        header.header_size = data - data_begin;

    return header;
}
