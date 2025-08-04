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
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/peer.hpp>
#include <ws-streaming/detail/streaming_protocol.hpp>
#include <ws-streaming/detail/websocket_protocol.hpp>

using namespace std::placeholders;

wss::detail::peer::peer(
        boost::asio::ip::tcp::socket&& socket,
        bool is_client,
        std::size_t rx_buffer_size,
        std::size_t tx_buffer_size)
    : _socket{std::move(socket)}
    , _rx_buffer(rx_buffer_size)
    , _tx_buffer(tx_buffer_size)
{
    _socket.non_blocking(true);
    set_send_buffer_size(tx_buffer_size);
}

void wss::detail::peer::run()
{
    do_wait_rx();
}

void wss::detail::peer::run(const void *data, std::size_t size)
{
    if (size > _rx_buffer.size())
        boost::asio::post(
            _socket.get_executor(),
            [self = shared_from_this()]()
            {
                self->close(boost::asio::error::no_buffer_space);
            });

    std::memcpy(
        _rx_buffer.data(),
        data,
        size);

    _rx_buffer_bytes = size;

    boost::asio::post(
        _socket.get_executor(),
        [self = shared_from_this()]()
        {
            self->process_buffer();
        });
}

void wss::detail::peer::stop()
{
    boost::system::error_code ec;
    _socket.close(ec);
}

void wss::detail::peer::send_metadata(
    unsigned signo,
    const std::string& method,
    const nlohmann::json& params)
{
    std::uint32_t encoding = detail::streaming_protocol::metadata_encoding::MSGPACK;

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
        detail::streaming_protocol::packet_type::METADATA,
        buffers,
        sizeof(encoding) + payload.size());
}

void wss::detail::peer::set_send_buffer_size(std::size_t size)
{
    // Clamp the requested tx buffer size to the largest value we can store in an int.
    if (size > std::numeric_limits<int>::max())
        size = std::numeric_limits<int>::max();

    // Ask the operating system to hold as much as possible.
    boost::system::error_code ec;
    auto option = boost::asio::socket_base::send_buffer_size{static_cast<int>(size)};
    _socket.set_option(option, ec);
}

void wss::detail::peer::do_wait_rx()
{
    _socket.async_wait(
        boost::asio::socket_base::wait_read,
        std::bind(
            &peer::finish_wait_rx,
            shared_from_this(),
            _1));
}

void wss::detail::peer::do_wait_tx()
{
    _socket.async_wait(
        boost::asio::socket_base::wait_write,
        std::bind(
            &peer::finish_wait_tx,
            shared_from_this(),
            _1));

    _waiting_tx = true;
}

void wss::detail::peer::finish_wait_rx(const boost::system::error_code& wait_ec)
{
    boost::system::error_code receive_ec;

    // Was there an error waiting for the socket to become readable?
    if (wait_ec)
        return close(wait_ec);

    // Since the socket is readable, read as much as we can from it.
    std::size_t bytes_received = _socket.receive(
        boost::asio::buffer(
            _rx_buffer.data() + _rx_buffer_bytes,
            _rx_buffer.size() - _rx_buffer_bytes),
        0,
        receive_ec);

    if (bytes_received == 0)
        return close({});

    // Was there a genuine error reading from the socket?
    if (receive_ec && receive_ec != boost::asio::error::would_block)
        return close(receive_ec);

    _rx_buffer_bytes += bytes_received;

    process_buffer();
}

void wss::detail::peer::finish_wait_tx(const boost::system::error_code& wait_ec)
{
    boost::system::error_code send_ec;
    _waiting_tx = false;

    // Was there an error waiting for the socket to become writeable?
    if (wait_ec)
        return close(wait_ec);

    // Since the socket is writeable, write as much as we can to it.
    std::size_t bytes_sent = _socket.send(
        boost::asio::buffer(_tx_buffer.data(), _tx_buffer_bytes),
        0,
        send_ec);

    // Was there a genuine error writing to the socket?
    if (send_ec && send_ec != boost::asio::error::would_block)
        return close(send_ec);

    // If we sent all the data in our buffer, we can stop now.
    if (bytes_sent)
        std::memmove(
            _tx_buffer.data(),
            &_tx_buffer[bytes_sent],
            _tx_buffer_bytes - bytes_sent);
    _tx_buffer_bytes -= bytes_sent;

    if (_shutdown_after)
    {
        _shutdown_after -= std::min(bytes_sent, _shutdown_after);
        if (_shutdown_after == 0)
            return close();
    }

    if (bytes_sent < _tx_buffer_bytes)
        do_wait_tx();
}

void wss::detail::peer::process_buffer()
{
    // Process as many WebSocket frames as possible.
    while (true)
    {
        // Try to decode the WebSocket header.
        auto header = detail::websocket_protocol::decode_header(
            _rx_buffer.data(),
            _rx_buffer_bytes);

        // If there's not enough data to form a complete frame, we can't process any more.
        if (!header.header_size)
            break;

        boost::system::error_code process_ec;
        process_websocket_frame(
            header,
            _rx_buffer.data() + header.header_size,
            header.payload_size,
            process_ec);

        if (process_ec)
            return close(process_ec);

        // Consume the handled frame by sliding the remaining data in the read buffer over
        // to the left. (Can't use std::memcpy() for this because the ranges overlap.)
        std::memmove(
            &_rx_buffer[0],
            &_rx_buffer[header.header_size + header.payload_size],
            _rx_buffer_bytes - header.header_size + header.payload_size);
        _rx_buffer_bytes -= header.header_size + header.payload_size;
    }

    // If the read buffer is still full after processing, it is an error condition
    // (the client must be sending a frame larger than our fixed-size read buffer).
    if (_rx_buffer_bytes == _rx_buffer.size())
        return close(boost::asio::error::no_buffer_space);

    do_wait_rx();
}

void wss::detail::peer::process_websocket_frame(
    const detail::websocket_protocol::decoded_header& header,
    std::uint8_t *data,
    std::size_t size,
    boost::system::error_code& ec)
{
    if (!(header.flags & detail::websocket_protocol::flags::FIN))
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
        case detail::websocket_protocol::opcodes::CLOSE:
        {
            send_websocket_frame(
                detail::websocket_protocol::opcodes::CLOSE,
                boost::asio::const_buffer(),
                0,
                true);
            break;
        }

        case detail::websocket_protocol::opcodes::PING:
        {
            send_websocket_frame(
                detail::websocket_protocol::opcodes::PONG,
                boost::asio::const_buffer(data, size),
                size);
            break;
        }

        case detail::websocket_protocol::opcodes::TEXT:
            break;

        case detail::websocket_protocol::opcodes::BINARY:
        {
            process_packet(data, size);
            break;
        }

        // React to any other frames by ignoring them.
        default:
            break;
    }
}

void wss::detail::peer::process_packet(
    const std::uint8_t *data,
    std::size_t size)
{
    // Try to decode the WebSocket Streaming Protocol packet.
    auto header = detail::streaming_protocol::decode_header(data, size);
    if (!header.header_size)
        return;

    switch (header.type)
    {
        case detail::streaming_protocol::packet_type::DATA:
            process_data_packet(
                header.signo,
                data + header.header_size,
                header.payload_size);
            break;

        case detail::streaming_protocol::packet_type::METADATA:
            process_metadata_packet(
                header.signo,
                data + header.header_size,
                header.payload_size);
            break;

        default:
            break;
    }
}

void wss::detail::peer::process_data_packet(
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    on_data_received(signo, data, size);
}

void wss::detail::peer::process_metadata_packet(
    unsigned signo,
    const std::uint8_t *data,
    std::size_t size)
{
    if (size < sizeof(std::uint32_t))
        return;

    std::uint32_t encoding = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    nlohmann::json metadata;
    switch (encoding)
    {
        case detail::streaming_protocol::metadata_encoding::MSGPACK:
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
                    metadata.contains("params")
                        ? metadata["params"]
                        : nlohmann::json{nullptr});
            break;

        default:
            break;
    }
}

void wss::detail::peer::close(
    const boost::system::error_code& ec)
{
    if (_is_closed)
        return;

    _is_closed = true;

    boost::system::error_code close_ec;
    _socket.shutdown(_socket.shutdown_both, close_ec);
    _socket.close(close_ec);

    on_closed(ec);
}
