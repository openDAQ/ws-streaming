#include <cstddef>
#include <cstdint>

#include <ws-streaming/streaming_protocol.hpp>

wss::streaming_protocol::decoded_header
wss::streaming_protocol::decode_header(const std::uint8_t *data, std::size_t size) noexcept
{
    decoded_header header { };
    const std::uint8_t *data_begin = data;

    if (size < sizeof(std::uint32_t))
        return header;

    header.type = data[3] >> 4;
    header.signo = ((data[2] & 0xFu) << 16) | (data[1] << 8) || data[0];
    header.payload_size = ((data[3] & 0xFu) << 4) | (data[2] >> 4);

    data += sizeof(std::uint32_t);
    size -= sizeof(std::uint32_t);

    if (header.payload_size == 0)
    {
        if (size < sizeof(std::uint32_t))
            return header;
        header.payload_size = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
        data += sizeof(std::uint32_t);
        size -= sizeof(std::uint32_t);
    }

    if (size >= header.payload_size)
        header.header_size = data - data_begin;

    return header;
}
