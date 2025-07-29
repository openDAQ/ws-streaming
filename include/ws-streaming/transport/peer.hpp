#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/detail/streaming_protocol.hpp>
#include <ws-streaming/detail/websocket_protocol.hpp>

namespace wss::transport
{
    /**
     * Implements the transport layer of a WebSocket Streaming Protocol connection to a remote
     * peer. Transport connections are symmetric, and support asynchronous, bidirectional exchange
     * of WebSocket Streaming Protocol packets. To send packets, callers use the send_data() and
     * send_metadata() functions. To receive packets, callers connect to the on_data_received and
     * on_metadata_received signals. The on_closed signal can be used to detect when the
     * connection has been closed, either gracefully or due to an error.
     *
     * Peer objects are constructed with, and take ownership of, a connected Boost.Asio socket,
     * and must always be managed by a std::shared_ptr, following the normal Boost.Asio pattern.
     * When run() is called, the peer object performs asynchronous I/O operations using the
     * execution context of the provided socket. This execution context must provide sequential
     * execution, i.e. in the terminology of Boost.Asio, it must be an explicit or implicit
     * strand. In addition, the caller must ensure no member functions are called concurrently
     * with each other or with an asynchronous completion handler. More explicitly stated, this
     * class is not thread-safe.
     *
     * Because peer objects must always be managed by a std::shared_ptr, a caller cannot directly
     * destroy a peer object. Calling the stop() function begins the process of destroying a peer:
     * all pending asynchronous I/O operations are canceled, and the on_closed signal is raised.
     * The peer is then destroyed when the caller releases all shared-pointer references to it and
     * once all asynchronous completion handlers have been called.
     *
     * @todo When acting as a client, outgoing WebSocket frames are not masked, in violation of
     *     section 5.3 of RFC 6455. This will not be compatible with third-party servers that
     *     enforce the masking requirement. Masking has a significant performance cost, in that it
     *     makes zero-copy impossible. Ideally, we should mask by default, but also negotiate with
     *     the server to disable masking if the server allows it.
     *
     * @todo This class is coupled with boost::asio::tcp and cannot currently be used with other
     *     stream socket types such as UNIX domain sockets. It should either be templated or use a
     *     polymorphic socket adapter so it can work with any stream socket type.
     */
    class peer : public std::enable_shared_from_this<peer>
    {
        public:

            /**
             * Constructs a peer object, taking ownership of the specified socket. No asynchronous
             * operations are started until run() is called. Asynchronous socket operations will
             * be dispatched using the socket's execution context.
             *
             * @param socket A socket, which the constructed object takes ownership of. The socket
             *     should be connected to the remote peer.
             * @param is_client True if this object should act as a client. This determines
             *     whether transmitted WebSocket frames are masked according to section 5.3 of RFC
             *     6455. The masking feature is not currently implemented.
             * @param rx_buffer_size The desired size of the receive buffer. The receive buffer
             *     does not grow, so this value sets an upper bound on the size of frames the peer
             *     can receive: larger frames will result in an error and cause the connection to
             *     be closed. The specified value does not include the operating system's internal
             *     receive buffer.
             * @param tx_buffer_size The desired size of the transmit buffer. The transmit buffer
             *     does not grow, so this value sets an upper bound on the size of frames the peer
             *     can transmit: larger frames will result in an error and cause the connection to
             *     be closed. For maximum performance, the implementation will place as much as
             *     possible of the requested size in the operating system's internal transmit
             *     buffer, because this minimizes the likelihood of needing to additionally buffer
             *     data in user-space. Increasing the operating system's configured maximum
             *     transmit buffer size can often significantly improve performance.
             *
             * @throws boost::system::system_error The socket could not be placed into
             *     non-blocking mode.
             */
            peer(
                boost::asio::ip::tcp::socket&& socket,
                bool is_client,
                std::size_t rx_buffer_size = 1024 * 1024,
                std::size_t tx_buffer_size = 32 * 1024 * 1024);

            /**
             * Activates the peer by starting asynchronous I/O operations using the socket's
             * execution context. To stop the peer later, call stop(), which cancels all
             * asynchronous I/O operations, allowing the object to be destroyed.
             */
            void run();

            /**
             * Activates the peer by starting asynchronous I/O operations using the socket's
             * execution context. The specified data is processed as if it had been received from
             * the socket. This is useful, for example, if an HTTP client has inadvertently read
             * and buffered WebSocket data after the HTTP request response. To stop the peer
             * later, call stop(), which cancels all asynchronous I/O operations, allowing the
             * object to be destroyed.
             *
             * Even if this function is called from within the correct execution context, the
             * processing of the specified data is deferred using boost::asio::post().
             *
             * @param data A pointer to the data to process.
             * @param size The number of bytes pointed to by @p data.
             */
            void run(const void *data, std::size_t size);

            /**
             * Closes the connection. The socket is closed, and any pending asynchronous socket
             * operations are canceled, but their completion handlers, which hold shared-pointer
             * references to this object, will be posted to the execution context and execute
             * later. The on_closed signal will be raised later from the execution context,
             * unless it has already been raised due to an error or a previous call to stop().
             */
            void stop();

            /**
             * Asynchronously sends signal data to the remote peer. This function directly
             * supports scatter-gather operations with no additional copy steps.
             *
             * @tparam ConstBufferSequence A type that satisfies the Boost.Asio requirements for
             *     a sequence of immutable buffers.
             *
             * @param signo The signal number to which the data applies.
             * @param data A sequence of Boost.Asio buffer descriptors for the data to send.
             */
            template <typename ConstBufferSequence>
            void send_data(
                unsigned signo,
                const ConstBufferSequence& data)
            {
                send_packet(
                    signo,
                    detail::streaming_protocol::packet_type::DATA,
                    data,
                    std::nullopt);
            }

            /**
             * Asynchronously sends JSON-RPC metadata to the remote peer.
             *
             * @param signo The signal number to which the metadata applies, or zero for global
             *     metadata.
             * @param method The metadata method name.
             * @param params A JSON value containing the metadata parameters.
             */
            void send_metadata(
                unsigned signo,
                const std::string& method,
                const nlohmann::json& params);

            /**
             * A signal raised when a WebSocket Streaming Protocol data packet is received. The
             * signal is raised from the execution context of the socket.
             *
             * @param signo The signal number to which the data applies.
             * @param data A pointer to the payload data.
             * @param size The number of payload data bytes pointed to by @p data.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(
                    unsigned signo,
                    const std::uint8_t *data,
                    std::size_t size)
            > on_data_received;

            /**
             * A signal raised when a WebSocket Streaming Protocol metadata packet is received.
             * The signal is raised from the execution context of the socket.
             *
             * @param signo The signal number to which the metadata applies, or zero for global
             *     metadata.
             * @param method The metadata method name.
             * @param params A JSON value containing the metadata parameters.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(
                    unsigned signo,
                    const std::string& method,
                    const nlohmann::json& params)
            > on_metadata_received;

            /**
             * A signal raised when the connection is closed. This can occur due to an error, or
             * when stop() is called. The signal is raised from the execution context of the
             * socket. Note that this may occur even after a caller has released its
             * std::shared_ptr reference to a peer object.
             *
             * @param ec The error code of the error, if any, that caused the connection to be
             *     closed.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
            > on_closed;

            /**
             * Gets the underlying socket.
             *
             * @return The underlying socket.
             */
            boost::asio::ip::tcp::socket& socket()
            {
                return _socket;
            }

        private:

            void set_send_buffer_size(std::size_t size);

            void do_wait_rx();
            void do_wait_tx();

            void finish_wait_rx(const boost::system::error_code& wait_ec);
            void finish_wait_tx(const boost::system::error_code& wait_ec);

            void process_buffer();

            void process_websocket_frame(
                const detail::websocket_protocol::decoded_header& header,
                std::uint8_t *data,
                std::size_t size,
                boost::system::error_code& ec);

            void process_packet(const std::uint8_t *data, std::size_t size);
            void process_data_packet(unsigned signo, const std::uint8_t *data, std::size_t size);
            void process_metadata_packet(unsigned signo, const std::uint8_t *data, std::size_t size);

            template <typename ConstBufferSequence>
            void send_packet(
                unsigned signo,
                unsigned type,
                const ConstBufferSequence& payload,
                const std::optional<std::size_t>& payload_size)
            {
                std::array<std::uint8_t, detail::streaming_protocol::MAX_HEADER_SIZE> streaming_header;

                std::size_t calculated_payload_size
                    = payload_size.has_value()
                        ? payload_size.value()
                        : boost::asio::buffer_size(payload);

                auto streaming_header_size = detail::streaming_protocol::generate_header(
                    streaming_header.data(),
                    signo,
                    type,
                    calculated_payload_size);

                send_websocket_frame(
                    detail::websocket_protocol::opcodes::BINARY,
                    boost::beast::buffers_cat(
                        boost::asio::buffer(
                            streaming_header.data(),
                            streaming_header_size),
                        payload),
                    streaming_header_size + calculated_payload_size);
            }

            template <typename ConstBufferSequence>
            void send_websocket_frame(
                unsigned opcode,
                const ConstBufferSequence& payload,
                const std::optional<std::size_t>& payload_size,
                bool do_shutdown_after = false)
            {
                std::array<std::uint8_t, detail::websocket_protocol::MAX_HEADER_SIZE> ws_header;

                std::size_t calculated_payload_size
                    = payload_size.has_value()
                        ? payload_size.value()
                        : boost::asio::buffer_size(payload);

                auto ws_header_size = detail::websocket_protocol::generate_header(
                    ws_header.data(),
                    opcode,
                    detail::websocket_protocol::flags::FIN,
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
            void write(
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
                if (_waiting_tx)
                    return enqueue(buffers, calculated_size, do_shutdown_after);

                // Otherwise, send as much as we can synchronously.
                boost::system::error_code send_ec;
                std::size_t bytes_sent = _socket.send(buffers, 0, send_ec);

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
            void enqueue(
                const ConstBufferSequence& buffers,
                std::size_t size,
                bool do_shutdown_after)
            {
                std::size_t bytes_buffered = boost::asio::buffer_copy(
                    boost::asio::mutable_buffer(
                        _tx_buffer.data() + _tx_buffer_bytes,
                        _tx_buffer.size() - _tx_buffer_bytes),
                    buffers);

                _tx_buffer_bytes += bytes_buffered;

                if (_tx_buffer_bytes == _tx_buffer.size())
                    return close(boost::asio::error::no_buffer_space);

                if (do_shutdown_after)
                    _shutdown_after = _tx_buffer_bytes;

                if (!_waiting_tx)
                    do_wait_tx();
            }

            void close(const boost::system::error_code& ec = {});

        private:

            boost::asio::ip::tcp::socket _socket;
            bool _is_closed = false;

            std::vector<std::uint8_t> _rx_buffer;
            std::vector<std::uint8_t> _tx_buffer;

            std::size_t _rx_buffer_bytes = 0;
            std::size_t _tx_buffer_bytes = 0;

            bool _waiting_tx = false;
            std::size_t _shutdown_after = 0;

            boost::system::error_code _close_ec;
    };
}
