#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/peer.hpp>
#include <ws-streaming/streaming_protocol.hpp>
#include <ws-streaming/websocket_protocol.hpp>

wss::peer::peer(
        boost::asio::ip::tcp::socket&& socket,
        bool is_client,
        std::size_t rx_buffer_size,
        std::size_t tx_buffer_size)
    : socket{std::move(socket)}
    , rx_buffer(rx_buffer_size)
    , tx_buffer(tx_buffer_size)
{
    this->socket.non_blocking(true);
    set_send_buffer_size(tx_buffer_size);
}

void wss::peer::run()
{
    do_wait_rx();
}

void wss::peer::stop()
{
    boost::system::error_code ec;
    socket.close(ec);
}

void wss::peer::send_metadata(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    std::uint32_t encoding = streaming_protocol::metadata_encoding::MSGPACK;

    auto payload = nlohmann::json::to_msgpack({
        {"method", method},
        {"params", params}
    });

    std::array<boost::asio::const_buffer, 2> buffers =
    {
        boost::asio::buffer(&encoding, sizeof(encoding)),
        boost::asio::buffer(payload),
    };

    send_packet(
        signo,
        streaming_protocol::packet_type::METADATA,
        buffers,
        sizeof(encoding) + payload.size());
}

void wss::peer::set_send_buffer_size(std::size_t size)
{
    // Clamp the requested tx buffer size to the largest value we can store in an int.
    if (size > std::numeric_limits<int>::max())
        size = std::numeric_limits<int>::max();

    // Ask the operating system to hold as much as possible.
    boost::system::error_code ec;
    auto option = boost::asio::socket_base::send_buffer_size{static_cast<int>(size)};
    socket.set_option(option, ec);
}

void wss::peer::do_wait_rx()
{
    socket.async_wait(
        boost::asio::socket_base::wait_read,
        std::bind(
            &peer::finish_wait_rx,
            shared_from_this(),
            std::placeholders::_1));
}

void wss::peer::do_wait_tx()
{
    socket.async_wait(
        boost::asio::socket_base::wait_write,
        std::bind(
            &peer::finish_wait_tx,
            shared_from_this(),
            std::placeholders::_1));

    waiting_tx = true;
}

void wss::peer::finish_wait_rx(const boost::system::error_code& wait_ec)
{
    boost::system::error_code receive_ec;

    // Was there an error waiting for the socket to become readable?
    if (wait_ec)
        return close(wait_ec);

    // Since the socket is readable, read as much as we can from it.
    std::size_t bytes_received = socket.receive(
        boost::asio::buffer(
            rx_buffer.data() + rx_buffer_bytes,
            rx_buffer.size() - rx_buffer_bytes),
        0,
        receive_ec);

    if (bytes_received == 0)
        return close({});

    // Was there a genuine error reading from the socket?
    if (receive_ec && receive_ec != boost::asio::error::would_block)
        return close(receive_ec);

    rx_buffer_bytes += bytes_received;

    // Process as many WebSocket frames as possible.
    while (true)
    {
        // Try to decode the WebSocket header.
        auto header = websocket_protocol::decode_header(rx_buffer.data(), rx_buffer_bytes);

        // If there's not enough data to form a complete frame, we can't process any more.
        if (!header.header_size)
            break;

        boost::system::error_code process_ec;
        process_websocket_frame(
            header,
            rx_buffer.data() + header.header_size,
            header.payload_size,
            process_ec);

        if (process_ec)
            return close(process_ec);

        // Consume the handled frame by sliding the remaining data in the read buffer over
        // to the left. (Can't use std::memcpy() for this because the ranges overlap.)
        std::memmove(
            &rx_buffer[0],
            &rx_buffer[header.header_size + header.payload_size],
            rx_buffer_bytes - header.header_size + header.payload_size);
        rx_buffer_bytes -= header.header_size + header.payload_size;
    }

    // If the read buffer is still full after processing, it is an error condition
    // (the client must be sending a frame larger than our fixed-size read buffer).
    if (rx_buffer_bytes == rx_buffer.size())
        return close(boost::asio::error::no_buffer_space);

    do_wait_rx();
}

void wss::peer::finish_wait_tx(const boost::system::error_code& wait_ec)
{
    boost::system::error_code send_ec;
    waiting_tx = false;

    // Was there an error waiting for the socket to become writeable?
    if (wait_ec)
        return close(wait_ec);

    // Since the socket is writeable, write as much as we can to it.
    std::size_t bytes_sent = socket.send(
        boost::asio::buffer(tx_buffer.data(), tx_buffer_bytes),
        0,
        send_ec);

    // Was there a genuine error writing to the socket?
    if (send_ec && send_ec != boost::asio::error::would_block)
        return close(send_ec);

    // If we sent all the data in our buffer, we can stop now.
    if (bytes_sent)
        std::memmove(
            tx_buffer.data(),
            &tx_buffer[bytes_sent],
            tx_buffer_bytes - bytes_sent);
    tx_buffer_bytes -= bytes_sent;

    if (shutdown_after)
    {
        shutdown_after -= std::min(bytes_sent, shutdown_after);
        if (shutdown_after == 0)
            return close();
    }

    if (bytes_sent < tx_buffer_bytes)
        do_wait_tx();
}

void wss::peer::process_websocket_frame(
    const websocket_protocol::decoded_header& header,
    std::uint8_t *data,
    std::size_t size,
    boost::system::error_code& ec)
{
    if (!(header.flags & websocket_protocol::flags::FIN))
    {
        ec = boost::asio::error::operation_not_supported;
        return;
    }

    if (header.is_masked)
        for (std::size_t i = 0; i < size; ++i)
            data[i] ^= header.masking_key[i % 4];

    // We have a valid and complete WebSocket frame.
    switch (header.opcode)
    {
        // React to close frames by sending our own close frame and then signaling the caller to disconnect.
        case websocket_protocol::opcodes::CLOSE:
        {
            send_websocket_frame(
                websocket_protocol::opcodes::CLOSE,
                boost::asio::const_buffer(),
                0,
                true);
            break;
        }

        case websocket_protocol::opcodes::PING:
        {
            send_websocket_frame(
                websocket_protocol::opcodes::PONG,
                boost::asio::const_buffer(data, size),
                size);
            break;
        }

        case websocket_protocol::opcodes::TEXT:
            break;

        case websocket_protocol::opcodes::BINARY:
        {
            process_packet(data, size);
            break;
        }

        // React to any other frames by ignoring them.
        default:
            break;
    }
}

void wss::peer::process_packet(const std::uint8_t *data, std::size_t size)
{
    // Try to decode the WebSocket Streaming Protocol packet.
    auto header = streaming_protocol::decode_header(data, size);
    if (!header.header_size)
        return;

    switch (header.type)
    {
        case streaming_protocol::packet_type::DATA:
            process_data_packet(
                header.signo,
                data + header.header_size,
                header.payload_size);
            break;

        case streaming_protocol::packet_type::METADATA:
            process_metadata_packet(
                header.signo,
                data + header.header_size,
                header.payload_size);
            break;

        default:
            break;
    }
}

void wss::peer::process_data_packet(unsigned signo, const std::uint8_t *data, std::size_t size)
{
    on_data_received(signo, data, size);
}

void wss::peer::process_metadata_packet(unsigned signo, const std::uint8_t *data, std::size_t size)
{
    if (size < sizeof(std::uint32_t))
        return;

    std::uint32_t encoding = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    nlohmann::json metadata;
    switch (encoding)
    {
        case streaming_protocol::metadata_encoding::MSGPACK:
            try
            {
                metadata = nlohmann::json::from_msgpack(
                    data + sizeof(encoding),
                    data + size);
            }
            catch (const nlohmann::json::exception&)
            {
                break;
            }
            if (metadata.is_object()
                    && metadata.contains("method")
                    && metadata["method"].is_string())
                on_metadata_received(
                    signo,
                    metadata["method"],
                    metadata.contains("params") ? metadata["params"] : nullptr);
            break;

        default:
            break;
    }
}

template <typename ConstBufferSequence>
void wss::peer::send_packet(
    unsigned signo,
    unsigned type,
    const ConstBufferSequence& payload,
    const std::optional<std::size_t>& payload_size)
{
    std::array<std::uint8_t, streaming_protocol::MAX_HEADER_SIZE> streaming_header;

    std::size_t calculated_payload_size
        = payload_size.has_value()
            ? payload_size.value()
            : boost::asio::buffer_size(payload);

    auto streaming_header_size = streaming_protocol::generate_header(
        streaming_header.data(),
        signo,
        type,
        calculated_payload_size);

    send_websocket_frame(
        websocket_protocol::opcodes::BINARY,
        boost::beast::buffers_cat(
            boost::asio::buffer(
                streaming_header.data(),
                streaming_header_size),
            payload),
        streaming_header_size + calculated_payload_size);
}

template <typename ConstBufferSequence>
void wss::peer::send_websocket_frame(
    unsigned opcode,
    const ConstBufferSequence& payload,
    const std::optional<std::size_t>& payload_size,
    bool do_shutdown_after)
{
    std::array<std::uint8_t, websocket_protocol::MAX_HEADER_SIZE> ws_header;

    std::size_t calculated_payload_size
        = payload_size.has_value()
            ? payload_size.value()
            : boost::asio::buffer_size(payload);

    auto ws_header_size = websocket_protocol::generate_header(
        ws_header.data(),
        opcode,
        websocket_protocol::flags::FIN,
        calculated_payload_size);

    write(
        boost::beast::buffers_cat(
            boost::asio::buffer(
                ws_header.data(),
                ws_header_size),
            payload),
        ws_header_size + calculated_payload_size,
        do_shutdown_after);
}

template <typename ConstBufferSequence>
void wss::peer::write(
    const ConstBufferSequence& buffers,
    const std::optional<std::size_t>& size,
    bool do_shutdown_after)
{
    std::size_t calculated_size
        = size.has_value()
            ? size.value()
            : boost::asio::buffer_size(buffers);

    // If we already have user-space buffered data and are waiting for the socket to
    // become writeable, we just need to add the additional data to the user-space
    // buffer; the wait completion handler will see the additional data along with
    // whatever was previously buffered.
    if (waiting_tx)
        return enqueue(buffers, calculated_size, do_shutdown_after);

    // Otherwise, send as much as we can synchronously.
    boost::system::error_code send_ec;
    std::size_t bytes_sent = socket.send(buffers, 0, send_ec);

    // Did a genuine error occur?
    if (send_ec && send_ec != boost::asio::error::would_block)
        return close(send_ec);

    // Did we synchronously send all the requested data?
    if (bytes_sent == calculated_size)
    {
        if (do_shutdown_after)
            return close();
        return;
    }

    // There is leftover data that could not be sent synchronously. Buffer up the
    // remaining data and start an asynchronous wait for the socket to become
    // writeable.
    boost::beast::buffers_suffix suffix{buffers};
    suffix.consume(bytes_sent);
    return enqueue(suffix, calculated_size - bytes_sent, do_shutdown_after);
}

template <typename ConstBufferSequence>
void wss::peer::enqueue(
    const ConstBufferSequence& buffers,
    std::size_t size,
    bool do_shutdown_after)
{
    std::size_t bytes_buffered = boost::asio::buffer_copy(
        boost::asio::mutable_buffer(
            tx_buffer.data() + tx_buffer_bytes,
            tx_buffer.size() - tx_buffer_bytes),
        buffers);

    tx_buffer_bytes += bytes_buffered;

    if (tx_buffer_bytes == tx_buffer.size())
        return close(boost::asio::error::no_buffer_space);

    if (do_shutdown_after)
        shutdown_after = tx_buffer_bytes;

    if (!waiting_tx)
        do_wait_tx();
}

void wss::peer::close(const boost::system::error_code& ec)
{
    if (!socket.is_open())
        return;

    boost::system::error_code close_ec;
    socket.shutdown(socket.shutdown_both, close_ec);
    socket.close(close_ec);

    on_closed(ec);
}
