#pragma once

#include <functional>
#include <memory>
#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>
#include <boost/url.hpp>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/http_client.hpp>

namespace wss
{
    /**
     * Asynchronously establishes a WebSocket Streaming connection by making an HTTP/WebSocket
     * request to a remote server. The caller calls async_connect() with a WebSocket URL. When the
     * WebSocket connection is established, or an error occurs, the completion handler passed to
     * async_connect() is called with the corresponding error code and/or a constructed
     * wss::connection object.
     *
     * A client object can perform multiple async_connect() calls, but only one connection attempt
     * at a time may be in progress. Connection attempts can be canceled by calling cancel(); the
     * completion handler will then be called with the error code
     * boost::asio::error::operation_aborted.
     *
     * Client instances must always be owned by a std::shared_ptr.
     */
    class client : public std::enable_shared_from_this<client>
    {
        public:

            /**
             * Constructs a client object. Asynchronous socket operations will be dispatched using
             * the specified execution context.
             *
             * @param executor An execution context to use for asynchronous I/O operations.
             */
            client(boost::asio::any_io_executor executor);

            /**
             * Asynchronously connects to a remote server. An HTTP GET request is made to
             * establish the WebSocket connection.
             *
             * @param url The WebSocket URL of the remote server.
             * @param handler A completion handler to call when the operation is complete. This
             *     handler receives either a nonzero error code, or a std::shared_ptr holding a
             *     constructed wss::connection object. The handler is guaranteed to be called
             *     exactly once, and to be dispatched using the execution context passed to the
             *     constructor.
             */
            void async_connect(
                const boost::urls::url_view& url,
                std::function<
                    void(
                        const boost::system::error_code& ec,
                        const std::shared_ptr<wss::connection>& connection)
                > handler);

            /**
             * Cancels a pending connection attempt, if any. The handler passed to async_connect()
             * will be called with the error code boost::asio::error::operation_aborted, if it has
             * not already been called. Note that it is possible a successful connection attempt
             * has already been scheduled with the execution context, resulting in the handler
             * being called with a successful connection even after calling this function.
             * Cancellation guarantees only that some call to the completion handler, successful
             * or otherwise, will be scheduled with the execution context.
             */
            void cancel();

        private:

            std::string get_random_key();

        private:

            std::shared_ptr<detail::http_client> _http_client;
    };
}
