#pragma once

#include <boost/signals2/connection.hpp>

#include <ws-streaming/connection.hpp>

namespace wss::detail
{
    struct connected_client
    {
        connected_client(connection_ptr connection)
            : connection(connection)
        {
        }

        connection_ptr connection;
        boost::signals2::scoped_connection on_available;
        boost::signals2::scoped_connection on_unavailable;
        boost::signals2::scoped_connection on_disconnected;
    };
}
