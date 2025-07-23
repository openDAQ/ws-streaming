#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

#include <boost/asio.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_suffix.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/peer.hpp>
#include <ws-streaming/streaming_protocol.hpp>
#include <ws-streaming/websocket_protocol.hpp>

wss::peer::peer(
        boost::asio::ip::tcp::socket socket,
        std::size_t rx_buffer_size,
        std::size_t tx_buffer_size)
    : socket{std::move(socket)}
    , rx_buffer(rx_buffer_size)
    , tx_buffer(tx_buffer_size)
{
    this->socket.non_blocking(true);

    // Ask the operating system to make the send buffer big enough to hold the entire
    // requested backlog (or at least the biggest value that will fit in an 'int').
    // TODO: Derate the user-space buffer according to how much the operating system
    // will give us.
    boost::system::error_code ec;
    this->socket.set_option(
        boost::asio::socket_base::send_buffer_size{
            static_cast<int>(
                std::min<std::size_t>(
                    tx_buffer_size,
                    std::numeric_limits<int>::max()))},
        ec);
}

void wss::peer::run()
{
    do_receive();
}

void wss::peer::close()
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
        buffers);
}

void wss::peer::do_receive()
{
    socket.async_receive(
        boost::asio::buffer(
            rx_buffer.data() + rx_buffer_bytes,
            rx_buffer.size() - rx_buffer_bytes),
        std::bind(
            &peer::finish_receive,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2));
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

void wss::peer::finish_receive(const boost::system::error_code& ec, std::size_t bytes_transferred)
{
    std::cout << "peer received " << bytes_transferred << " (ec " << ec << ')' << std::endl;

    if (ec)
        return close(ec);

    rx_buffer_bytes += bytes_transferred;

    // Process as many WebSocket frames as possible.
    while (true)
    {
        // Try to decode the WebSocket header.
        auto header = websocket_protocol::decode_header(rx_buffer.data(), rx_buffer_bytes);

        // If there's not enough data to form a complete frame, we can't process any more.
        if (!header.header_size)
            break;

        // We have a valid and complete WebSocket frame.
        switch (header.opcode)
        {
            // React to close frames by sending our own close frame and then signaling the caller to disconnect.
            case websocket_protocol::opcodes::CLOSE:
            {
                std::cout << "peer received close frame" << std::endl;
                // XXX TODO
                //std::array<std::uint8_t, 2> close_frame = { 0x88, 0x00 };
                //return socket.async_send(
                //    boost::asio::buffer(close_frame),
                //    std::bind(&peer::finish_close_write, shared_from_this(), _1, _2));
            }

            // React to any other frames by ignoring them.
            default:
                break;
        }

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

    do_receive();
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

    if (bytes_sent < tx_buffer_bytes)
        do_wait_tx();
}

template <typename ConstBufferSequence>
void wss::peer::send_packet(
    unsigned signo,
    unsigned type,
    const ConstBufferSequence& payload)
{
    std::array<std::uint8_t, streaming_protocol::MAX_HEADER_SIZE> streaming_header;
    std::array<std::uint8_t, websocket_protocol::MAX_HEADER_SIZE> ws_header;

    auto payload_size = boost::asio::buffer_size(payload);

    auto streaming_header_size = streaming_protocol::generate_header(
        streaming_header.data(),
        signo,
        type,
        payload_size);

    auto ws_header_size = websocket_protocol::generate_header(
        ws_header.data(),
        websocket_protocol::opcodes::BINARY,
        websocket_protocol::flags::FIN,
        streaming_header_size + payload_size);

    std::array<boost::asio::const_buffer, 4> buffers =
    {
        boost::asio::buffer(ws_header.data(), ws_header_size),
        boost::asio::buffer(streaming_header.data(), streaming_header_size),
    };

    write(
        boost::beast::buffers_cat(
            buffers,
            payload));
}

template <typename ConstBufferSequence>
boost::system::error_code
wss::peer::write(const ConstBufferSequence& buffers)
{
    // If we already have user-space buffered data and are waiting for the socket to
    // become writeable, we just need to add the additional data to the user-space
    // buffer; the wait completion handler will see the additional data along with
    // whatever was previously buffered.
    if (waiting_tx)
        return enqueue(buffers);

    // Otherwise, send as much as we can synchronously.
    boost::system::error_code send_ec;
    std::size_t bytes_sent = socket.send(buffers, 0, send_ec);

    // Did a genuine error occur?
    if (send_ec && send_ec != boost::asio::error::would_block)
        return send_ec;

    // Did we synchronously send all the requested data?
    if (bytes_sent == boost::asio::buffer_size(buffers))
        return boost::system::error_code{};

    // There is leftover data that could not be sent synchronously. Buffer up the
    // remaining data and start an asynchronous wait for the socket to become
    // writeable.
    boost::beast::buffers_suffix suffix{buffers};
    suffix.consume(bytes_sent);
    return enqueue(suffix);
}

template <typename ConstBufferSequence>
boost::system::error_code
wss::peer::enqueue(const ConstBufferSequence& buffers)
{
    std::size_t bytes_buffered = boost::asio::buffer_copy(
        boost::asio::mutable_buffer(
            tx_buffer.data() + tx_buffer_bytes,
            tx_buffer.size() - tx_buffer_bytes),
        buffers);

    tx_buffer_bytes += bytes_buffered;

    if (tx_buffer_bytes == tx_buffer.size())
        return boost::asio::error::no_buffer_space;

    if (!waiting_tx)
        do_wait_tx();

    return boost::system::error_code{};
}

void wss::peer::close(const boost::system::error_code& ec)
{
    if (socket.is_open())
        return;

    socket.close();
    on_disconnected(ec);
}
