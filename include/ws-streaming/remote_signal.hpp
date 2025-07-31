#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/metadata.hpp>

namespace wss
{
    class remote_signal
    {
        public:

            virtual void subscribe() = 0;
            virtual void unsubscribe() = 0;

            boost::signals2::signal<void()> on_subscribed;
            boost::signals2::signal<void()> on_unsubscribed;
            boost::signals2::signal<void()> on_metadata_changed;
            boost::signals2::signal<void(std::int64_t domain_value, const void *data, std::size_t size)> on_data_received;
            boost::signals2::signal<void()> on_unavailable;

            const std::string& id() const noexcept;
            bool is_subscribed() const noexcept;
            unsigned signo() const noexcept;
            const wss::metadata& metadata() const noexcept;

        protected:

            remote_signal(const std::string& id);

        protected:

            bool _is_subscribed = false;
            unsigned _signo = 0;
            wss::metadata _metadata;

        private:

            std::string _id;
    };
}
