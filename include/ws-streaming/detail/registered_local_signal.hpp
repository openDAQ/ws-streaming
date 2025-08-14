#pragma once

#include <cstdint>
#include <optional>

#include <boost/signals2/connection.hpp>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/detail/linear_table.hpp>

namespace wss::detail
{
    struct registered_local_signal
    {
        registered_local_signal(local_signal& signal, unsigned signo)
            : signal(signal)
            , signo(signo)
        {
        }

        local_signal&                           signal;
        unsigned                                signo;
        bool                                    is_explicitly_subscribed    = false;
        unsigned                                implicit_subscribe_count    = 0;
        boost::signals2::scoped_connection      on_metadata_changed;
        boost::signals2::scoped_connection      on_data_published;
        local_signal::subscribe_holder          holder;
        std::shared_ptr<detail::linear_table>   table;
        std::int64_t                            value_index                 = 0;
        unsigned                                domain_signo;
        std::weak_ptr<detail::linear_table>     domain_table;
        bool                                    is_explicit                 = false;
    };
}
