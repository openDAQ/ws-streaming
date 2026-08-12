#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <boost/range/adaptor/map.hpp>
#include <boost/signals2/connection.hpp>

#include <ws-streaming/detail/remote_signal_impl.hpp>

namespace wss::detail
{
    class remote_signal_container
    {
        protected:

            struct remote_signal_entry
            {
                remote_signal_entry(const std::string& id, bool hidden)
                    : hidden(hidden)
                    , signal(std::make_shared<detail::remote_signal_impl>(id))
                {
                }

                bool hidden = false;
                std::shared_ptr<detail::remote_signal_impl> signal;
                boost::signals2::scoped_connection on_subscribe_requested;
                boost::signals2::scoped_connection on_unsubscribe_requested;
                boost::signals2::scoped_connection on_table_sought;
            };

        protected:

            std::pair<bool, remote_signal_entry&> add_remote_signal(
                const std::string& id,
                bool hidden);

            remote_signal_entry *find_remote_signal(const std::string& id);
            remote_signal_entry *find_table(const std::string& table_id);
            remote_signal_entry *find_remote_signal(unsigned signo);

            const remote_signal_entry *find_remote_signal(const std::string& id) const;
            const remote_signal_entry *find_table(const std::string& table_id) const;
            const remote_signal_entry *find_remote_signal(unsigned signo) const;

            template <typename Func>
            void clear_remote_signals(Func&& func)
            {
                std::scoped_lock lock(_mutex);

                decltype(_signals_by_id) old_signals;
                std::swap(old_signals, _signals_by_id);
                _signals_by_signo.clear();

                for (const auto& signal : old_signals)
                    signal.second.signal->detach();

                for (const auto& signal : old_signals)
                    if (!signal.second.hidden)
                        func(signal.second.signal);
            }

            void forget_remote_signo(unsigned signo)
            {
                std::scoped_lock lock(_mutex);
                _signals_by_signo.erase(signo);
            }

            std::shared_ptr<detail::remote_signal_impl>
            remove_remote_signal(const std::string& id)
            {
                std::scoped_lock lock(_mutex);

                auto it = _signals_by_id.find(id);
                if (it == _signals_by_id.end())
                    return nullptr;

                auto signal = it->second.signal;
                _signals_by_id.erase(it);
                _signals_by_signo.erase(signal->remote_signal::signo());

                return signal;
            }

            void set_remote_signal_signo(remote_signal_entry *entry, unsigned signo)
            {
                std::scoped_lock lock(_mutex);
                _signals_by_signo[signo] = entry;
                entry->signal->signo(signo);
            }

        private:

            mutable std::mutex _mutex;
            std::map<std::string, remote_signal_entry> _signals_by_id;
            std::map<unsigned, remote_signal_entry *> _signals_by_signo;
    };
}
