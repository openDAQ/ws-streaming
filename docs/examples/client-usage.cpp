#include <ws-streaming/ws-streaming.hpp>

wss::client client{ioc};

client.async_connect(
    "ws://localhost:7414",
    [](const boost::system::error_code& ec, connection_ptr connection)
    {
        if (ec) // error occurred
            return;

        // WebSocket connection is successfully established
    });
