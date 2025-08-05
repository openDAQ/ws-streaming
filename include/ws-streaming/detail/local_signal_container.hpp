#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include <boost/range/adaptor/map.hpp>
#include <boost/signals2/connection.hpp>

#include <ws-streaming/local_signal.hpp>

namespace wss::detail
{
    class local_signal_container
    {
        protected:

            struct local_signal_entry
            {
                local_signal_entry(local_signal& signal, unsigned signo)
                    : signal(signal)
                    , signo(signo)
                {
                }

                local_signal& signal;
                unsigned signo;
                bool is_explicitly_subscribed = false;
                unsigned implicit_subscribe_count = 0;
                boost::signals2::scoped_connection on_metadata_changed;
                boost::signals2::scoped_connection on_data_published;
                std::int64_t linear_value = 0;
                local_signal::subscribe_holder holder;
            };

        protected:

            std::pair<unsigned, bool> add_local_signal(local_signal& signal);
            unsigned remove_local_signal(local_signal& signal);
            void clear_local_signals();

            local_signal_entry *find_local_signal(const std::string& id);
            local_signal_entry *find_local_signal(unsigned signo);

            auto local_signals()
            {
                return _signals | boost::adaptors::map_values;
            }

        private:

            std::map<unsigned, local_signal_entry> _signals;
            unsigned _next_signo = 1;
    };
}
