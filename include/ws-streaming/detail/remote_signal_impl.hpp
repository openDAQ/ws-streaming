#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/remote_signal.hpp>
#include <ws-streaming/detail/linear_table.hpp>

namespace wss::detail
{
    class remote_signal_impl : public remote_signal
    {
        public:

            remote_signal_impl(const std::string& id);

            void subscribe() override;

            void unsubscribe() override;

            void handle_data(
                const void *data,
                std::size_t size);

            void handle_metadata(
                const std::string& method,
                const nlohmann::json& params);

            void detach();

            void signo(unsigned signo);

            boost::signals2::signal<void()> on_subscribe_requested;
            boost::signals2::signal<void()> on_unsubscribe_requested;
            boost::signals2::signal<std::shared_ptr<remote_signal_impl>(const std::string& id)> on_signal_sought;

        private:

            void handle_subscribe();
            void handle_unsubscribe();
            void handle_signal(const nlohmann::json& params);

        private:

            unsigned _subscription_count = 0;

            std::shared_ptr<linear_table> _table;
            std::weak_ptr<linear_table> _domain_table;

            std::shared_ptr<remote_signal_impl> _domain_signal;

            bool _is_explicit = false;
            std::size_t _sample_size = 0;
            std::int64_t _value_index = 0;
    };
}
