#pragma once

#include <functional>
#include <memory>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/system/error_code.hpp>

namespace wss
{
    /**
     * Asynchronously listens for incoming socket connections, and notifies the caller by raising
     * a signal.
     *
     * Listener objects own a Boost.Asio socket, and must always be managed by a std::shared_ptr,
     * following the normal Boost.Asio pattern. When run() is called, the listener object performs
     * asynchronous I/O operations using the execution context passed to the constructor. This
     * execution context must provide sequential execution, i.e. in the terminology of Boost.Asio,
     * it must be an explicit or implicit strand. In addition, the caller must ensure no member
     * functions are called concurrently with each other or with an asynchronous completion
     * handler. More explicitly stated, this class is not thread-safe.
     *
     * Because listener objects must always be managed by a std::shared_ptr, a caller cannot
     * directly destroy a listener object. Calling the stop() function begins the process of
     * destroying a listener: all pending asynchronous I/O operations are canceled. The listener
     * is then destroyed when the caller releases all shared-pointer references to it and once all
     * asynchronous completion handlers have been called.
     *
     * @tparam Protocol A Boost.Asio protocol object which specifies the types of the socket and
     *     address objects to be used.
     */
    template <typename Protocol = boost::asio::ip::tcp>
    class listener : public std::enable_shared_from_this<listener<Protocol>>
    {
        public:

            listener(
                    boost::asio::any_io_executor executor,
                    typename Protocol::endpoint endpoint)
                : acceptor(executor)
            {
                boost::system::error_code ec;

                acceptor.open(endpoint.protocol(), ec);
                if (ec)
                    return;

                acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);

                acceptor.bind(endpoint, ec);
                if (ec)
                    return;

                acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
            }

            void run()
            {
                do_accept();
            }

            /**
             * Stops the listener. Any pending asynchronous I/O operations are canceled, but their
             * completion handlers, which hold shared-pointer references to this object, will be
             * posted to the execution context and execute later.
             */
            void stop()
            {
                acceptor.cancel();
            }

            /**
             * A signal raised when a new connection is accepted.
             *
             * @param client The socket for the client connection. Connected slots may take
             *     take ownership of (move-from) this object. Subsequent slots will receive a
             *     reference to a closed (moved-from) socket. If no connected slot moves-from this
             *     socket, it will be closed when all slots have finished executing.
             *
             * @throws ... Connected slots should not throw exceptions. If they do, they will
             *     propagate out to the execution context. This can result in an unhandled
             *     exception on a thread and terminate the process.
             */
            boost::signals2::signal<
                void(typename Protocol::socket& client)
            > on_accept;

        private:

            void do_accept()
            {
                using namespace std::placeholders;
                acceptor.async_accept(
                    std::bind(&listener::finish_accept, this->shared_from_this(), _1, _2));
            }

            void finish_accept(
                const boost::system::error_code& ec,
                typename Protocol::socket socket)
            {
                if (ec)
                    return;

                on_accept(socket);
                do_accept();
            }

            typename Protocol::acceptor acceptor;
    };
}
