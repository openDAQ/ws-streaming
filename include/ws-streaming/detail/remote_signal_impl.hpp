#pragma once

#include <cstddef>
#include <string>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/remote_signal.hpp>

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

            boost::signals2::signal<void()> on_subscribe_requested;
            boost::signals2::signal<void()> on_unsubscribe_requested;

        private:

            unsigned _subscription_count = 0;
    };
}
