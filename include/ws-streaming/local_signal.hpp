#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/metadata.hpp>

namespace wss
{
    class local_signal
    {
        public:

            local_signal(const std::string& id)
                : _id(id)
            {
            }

            void set_metadata(const nlohmann::json& metadata)
            {
                _metadata = metadata;

                _linear_start_delta = _metadata.linear_start_delta();
                _table_id = _metadata.table_id();

                on_metadata_changed(_metadata);
            }

            void publish_data(
                const void *data,
                std::size_t size)
            {
                on_data_published(0, 0, data, size);
            }

            void publish_data(
                std::int64_t domain_value,
                std::size_t sample_count,
                const void *data,
                std::size_t size)
            {
                on_data_published(domain_value, sample_count, data, size);
                _sample_index += sample_count;
            }

            const std::string& id() const noexcept
            {
                return _id;
            }

            const std::pair<std::int64_t, std::int64_t> linear_start_delta() const noexcept
            {
                return _linear_start_delta;
            }

            const wss::metadata& metadata() const noexcept
            {
                return _metadata;
            }

            std::size_t sample_index() const noexcept
            {
                return _sample_index;
            }

            const std::string& table_id() const noexcept
            {
                return _table_id;
            }

            boost::signals2::signal<
                void(const wss::metadata& metadata)
            > on_metadata_changed;

            boost::signals2::signal<
                void(
                    std::int64_t domain_value,
                    std::size_t sample_count,
                    const void *data,
                    std::size_t size)
            > on_data_published;

        private:

            std::string _id;
            std::pair<std::int64_t, std::int64_t> _linear_start_delta;
            wss::metadata _metadata;
            std::string _table_id;
            std::size_t _sample_index;
    };
}
