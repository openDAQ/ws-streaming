#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace wss
{
    class metadata
    {
        public:

            metadata();

            metadata(const nlohmann::json& json);

            std::string data_type() const;

            bool is_linear_rule() const;

            std::pair<std::int64_t, std::int64_t> linear_start_delta() const;

            std::string table_id() const;

            const nlohmann::json& json() const noexcept;

        private:

            nlohmann::json _json;
    };
}
