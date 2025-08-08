#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/dimension_builder.hpp>
#include <ws-streaming/rule_types.hpp>

wss::dimension_builder::dimension_builder(
        const std::string& name)
    : _dimension(
        {
            { "name", name },
            { "rule", rule_types::explicit_rule }
        })
{
}

wss::dimension_builder& wss::dimension_builder::linear_rule(
    std::int64_t start,
    std::int64_t delta,
    std::uint64_t size)
{
    _dimension["rule"] = rule_types::linear_rule;

    _dimension["linear"] =
    {
        { "start", start },
        { "delta", delta },
        { "size", size },
    };

    return *this;
}

const nlohmann::json& wss::dimension_builder::build() const noexcept
{
    return _dimension;
}
