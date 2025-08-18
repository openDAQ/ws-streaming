#pragma once

#include <list>

#include <ws-streaming/connection.hpp>
#include <ws-streaming/detail/connected_client.hpp>

namespace wss::detail
{
    class connected_client_iterator
    {
        public:

            connected_client_iterator() noexcept
            {
            }

            connected_client_iterator(std::list<detail::connected_client>::iterator it) noexcept
                : _it{it}
            {
            }

            connection_ptr& operator*() noexcept
            {
                return _it->connection;
            }

            connected_client_iterator& operator++() noexcept
            {
                ++_it;
                return *this;
            }

            bool operator!=(const connected_client_iterator& rhs) noexcept
            {
                return _it != rhs._it;
            }

        private:

            std::list<detail::connected_client>::iterator _it;
    };
}
