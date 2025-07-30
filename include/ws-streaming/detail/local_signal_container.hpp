#pragma once

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
                bool is_subscribed = false;
                boost::signals2::scoped_connection on_metadata_changed;
                boost::signals2::scoped_connection on_data;
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
