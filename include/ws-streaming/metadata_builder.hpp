#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace wss
{
    class metadata_builder
    {
        public:

            metadata_builder(
                const std::string& name,
                const std::string& id);

            metadata_builder(
                const nlohmann::json& metadata);

            metadata_builder& data_type(
                const std::string& type);

            metadata_builder& linear_rule(
                std::int64_t start,
                std::int64_t delta);

            metadata_builder& origin(
                const std::string& origin);

            metadata_builder& range(
                double low,
                double high);

            metadata_builder& resolution(
                std::uint64_t numerator,
                std::uint64_t denominator);

            metadata_builder& table(
                const std::string& id);

            metadata_builder& unit(
                int id,
                const std::string& name,
                const std::string& quantity,
                const std::string& symbol);

            const nlohmann::json& build() const noexcept;

        private:

            nlohmann::json _metadata;
    };
}
