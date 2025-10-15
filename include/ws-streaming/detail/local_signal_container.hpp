#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <boost/range/adaptor/map.hpp>
#include <boost/signals2/connection.hpp>

#include <ws-streaming/local_signal.hpp>
#include <ws-streaming/detail/registered_local_signal.hpp>

namespace wss::detail
{
    class local_signal_container
    {
        protected:

            std::pair<std::shared_ptr<registered_local_signal>, bool> add_local_signal(local_signal& signal);
            unsigned remove_local_signal(local_signal& signal);
            void clear_local_signals();

            std::shared_ptr<registered_local_signal>
            find_local_signal(const std::string& id);

            std::shared_ptr<registered_local_signal>
            find_local_signal(unsigned signo);

            auto local_signals()
            {
                return _signals | boost::adaptors::map_values;
            }

        private:

            std::map<unsigned, std::shared_ptr<registered_local_signal>> _signals;
            unsigned _next_signo = 1;
    };
}
