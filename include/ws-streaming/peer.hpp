#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/streaming_protocol.hpp>

namespace wss
{
    /**
     * Implements the transport layer of a WebSocket Streaming connection to a remote peer.
     * Transport connections are symmetric, and support asynchronous, bidirectional exchange of
     * WebSocket Streaming Protocol packets. To send packets, callers use the send() function. To
     * receive packets, callers connect to the on_metadata_received and on_data_received signals.
     *
     * Peer objects are constructed with, and take ownership of, a connected Boost.Asio socket,
     * and must always be managed by a std::shared_ptr. The on_disconnected signal can be used to
     * detect when the connection has been closed, whether gracefully or due to an error.
     */
    class peer : public std::enable_shared_from_this<peer>
    {
        public:

            /**
             * The default value for the receive buffer size passed to the constructor.
             */
            static constexpr std::size_t default_rx_buffer_size = 1024 * 1024;

            /**
             * The default value for the transmit buffer size passed to the constructor.
             */
            static constexpr std::size_t default_tx_buffer_size = 32 * 1024 * 1024;

            /**
             * Constructs a peer object, taking ownership of the specified socket. Asynchronous
             * socket operations will be dispatched using the socket's execution context.
             *
             * @param socket A socket which should be connected to the remote peer.
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
                boost::asio::ip::tcp::socket socket,
                std::size_t rx_buffer_size = default_rx_buffer_size,
                std::size_t tx_buffer_size = default_tx_buffer_size);

            void run();

            /**
             * Closes the connection. The socket is closed, and any pending asynchronous socket
             * operations are canceled, but their completion handlers, which hold shared-pointer
             * references to this object, will be posted to the execution context and execute
             * later. The on_disconnected signal will be raised later from the execution context,
             * unless it has already been raised due to an error or a previous call to close().
             *
             * @todo Implement graceful closure using a WebSocket close frame and socket shutdown.
             */
            void close();

            /**
             * Asynchronously sends a JSON-RPC control request to the remote peer.
             *
             * @param method The method to invoke over JSON-RPC.
             * @param params A JSON value containing the parameters to pass to the requested
             *     method.
             *
             * @throws XXX TODO @todo
             */
            void send_metadata(
                unsigned signo,
                const std::string& method,
                const nlohmann::json& params);

            template <typename ConstBufferSequence>
            void send_data(
                unsigned signo,
                const ConstBufferSequence& data)
            {
                send_packet(
                    signo,
                    streaming_protocol::packet_type::DATA,
                    data);
            }

            /**
             * A signal raised when a WebSocket Streaming Protocol metadata packet is received.
             * The signal is raised from the execution context of the socket.
             *
             * @param signo The signal number to which the metadata applies.
             * @param method A reference to the metadata method name.
             * @param params A reference to a JSON value containing the metadata parameters.
             *
             * @throws Any exception thrown by a connected slot is silently ignored.
             */
            boost::signals2::signal<
                void(
                    unsigned signo,
                    const std::string& method,
                    const nlohmann::json& params)
            > on_metadata_received;

            /**
             * A signal raised when a WebSocket Streaming Protocol data packet is received. The
             * signal is raised from the execution context of the socket.
             *
             * @param signo The signal number to which the data applies.
             * @param data A pointer to the payload data.
             * @param size The number of payload data bytes pointed to by @p data.
             *
             * @throws Any exception thrown by a connected slot is silently ignored.
             */
            boost::signals2::signal<
                void(
                    unsigned signo,
                    const void *data,
                    std::size_t size)
            > on_data_received;

            /**
             * A signal raised when the connection is closed. This can occur due to an error, or
             * when close() is called. The signal is raised from the execution context of the
             * socket.
             *
             * @param ec The error code of the error, if any, that caused the connection to be
             *     closed.
             */
            boost::signals2::signal<
                void(const boost::system::error_code& ec)
            > on_disconnected;

        private:

            void do_receive();
            void do_wait_tx();

            void finish_receive(const boost::system::error_code& ec, std::size_t bytes_transferred);
            void finish_wait_tx(const boost::system::error_code& wait_ec);

            template <typename ConstBufferSequence>
            void send_packet(
                unsigned signo,
                unsigned type,
                const ConstBufferSequence& payload);

            template <typename ConstBufferSequence>
            boost::system::error_code write(const ConstBufferSequence& buffers);

            template <typename ConstBufferSequence>
            boost::system::error_code enqueue(const ConstBufferSequence& buffers);

            void close(const boost::system::error_code& ec);

            boost::asio::ip::tcp::socket socket;

            std::vector<std::uint8_t> rx_buffer;
            std::vector<std::uint8_t> tx_buffer;

            std::size_t rx_buffer_bytes = 0;
            std::size_t tx_buffer_bytes = 0;

            bool waiting_tx = false;

            boost::system::error_code close_ec;
    };
}
