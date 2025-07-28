#pragma once

#include <cstddef>
#include <cstdint>

#include <boost/endian/conversion.hpp>

namespace wss::detail
{
    /**
     * Contains constants and other definitions related to the WebSocket Streaming Protocol.
     */
    namespace streaming_protocol
    {
        constexpr std::uint16_t DEFAULT_WEBSOCKET_PORT = 7414;              /**< The default TCP port for WebSocket connections. */
        constexpr std::uint16_t DEFAULT_CONTROL_PORT = 7438;                /**< The default TCP port for HTTP control channel connections. */
        constexpr std::size_t MAX_HEADER_SIZE = 2 * sizeof(std::uint32_t);  /**< The maximum possible packet header size, in bytes. */

        /**
         * Constants used in a WebSocket Streaming Protocol packet header to identify the type of the
         * packet's contents.
         */
        namespace packet_type
        {
            constexpr unsigned DATA = 1;        /**< Specifies that a packet contains signal data. */
            constexpr unsigned METADATA = 2;    /**< Specifies that a packet contains metadata. */
        }

        /**
         * Constants used in a WebSocket Streaming Protocol metadata packet header to identify the
         * way the metadata is encoded.
         */
        namespace metadata_encoding
        {
            constexpr unsigned MSGPACK = 2;     /**< Specifies that the metadata is MessagePack-encoded. */
        }

        /**
         * Populates a WebSocket Streaming Protocol packet header.
         *
         * @perfcrit This function is called once for every transmitted WebSocket Streaming
         *     Protocol packet.
         *
         * @param header A pointer to memory to populate with the header. The pointed-to area must
         *     be large enough to hold the largest possible header (MAX_HEADER_SIZE).
         * @param signo The signal number, which may be zero for metadata.
         * @param type The type of packet; see packet_type for possible values.
         * @param payload_size The size of the payload in bytes.
         *
         * @return The size of the generated header in bytes.
         */
        inline std::size_t generate_header(std::uint8_t *header,
            unsigned signo, unsigned type, std::size_t payload_size)
        {
            if (payload_size < 256)
            {
                reinterpret_cast<std::uint32_t *>(header)[0] = boost::endian::native_to_little<std::uint32_t>(signo | (payload_size << 20) | (type << 28));
                return sizeof(std::uint32_t);
            }

            else
            {
                reinterpret_cast<std::uint32_t *>(header)[0] = boost::endian::native_to_little<std::uint32_t>(signo | (type << 28));
                reinterpret_cast<std::uint32_t *>(header)[1] = boost::endian::native_to_little<std::uint32_t>(payload_size);
                return 2 * sizeof(std::uint32_t);
            }
        }

        /**
         * A structure containing values from a decoded WebSocket Streaming Protocol packet
         * header. The decode_header() function populates and returns an instance of this
         * structure.
         */
        struct decoded_header
        {
            std::size_t header_size;                    /**< The size of the header in bytes. */
            unsigned signo;                             /**< The signal number, which may be zero for metadata. */
            unsigned type;                              /**< The type of packet; see packet_type for possible values. */
            std::size_t payload_size;                   /**< The claimed payload size in bytes. */
        };

        /**
         * Decodes a WebSocket Streaming Protocol packet header.
         *
         * @param data A pointer to the WebSocket Streaming Protocol packet data. The data may be
         *     truncated; i.e., it is safe to call this function even if it's not known whether
         *     the data contains a complete and valid packet. In this case the returned
         *     decoded_header::header_size member is set to 0 (see the Returns description).
         * @param size The size of the data pointed to by @p data in bytes.
         *
         * @return A decoded_header structure containing the values of the packet's fields. If the
         *     pointed-to data contains a complete packet (including payload), the returned
         *     decoded_header::header_size member is set to the actual size of the header. If the
         *     data is truncated, the returned decoded_header::header_size member is set to 0.
         */
        decoded_header decode_header(const std::uint8_t *data, std::size_t size) noexcept;
    }
}
