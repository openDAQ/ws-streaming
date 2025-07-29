#pragma once

#include <cstddef>
#include <string>

#include <boost/signals2/signal.hpp>

#include <nlohmann/json.hpp>

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
                on_metadata_changed(_metadata);
            }

            const std::string& id() const noexcept
            {
                return _id;
            }

            const nlohmann::json& metadata() const noexcept
            {
                return _metadata;
            }

            boost::signals2::signal<
                void(const nlohmann::json& metadata)
            > on_metadata_changed;

            boost::signals2::signal<
                void(const void *data, std::size_t size)
            > on_data;

        private:

            std::string _id;
            nlohmann::json _metadata;
    };
}
