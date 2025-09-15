#include <iostream>
#include <optional>
#include <string>
#include <tuple>

#include <nlohmann/json.hpp>

#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/struct_field_dimension.hpp>

wss::struct_field_dimension::struct_field_dimension()
    : _json(nlohmann::json::object())
{
}

wss::struct_field_dimension::struct_field_dimension(const nlohmann::json& json)
    : _json(json.is_object() ? json : nlohmann::json::object())
{
    std::cout << "struct_field dimension constructed with " << json.dump() << std::endl;
}

std::tuple<
    std::optional<std::int64_t>,
    std::optional<std::int64_t>,
    std::optional<std::int64_t>
> wss::struct_field_dimension::linear_start_delta_size() const
{
    if (rule() != rule_types::linear_rule)
        return std::make_tuple(std::nullopt, std::nullopt, std::nullopt);

    if (_json.contains("linear")
        && _json["linear"].is_object())
    {
        const auto& linear = _json["linear"];

        std::optional<std::int64_t> start;
        if (auto element = linear.value<nlohmann::json>("start", nullptr);
                element.is_number())
            start = element;

        std::optional<std::int64_t> delta;
        if (auto element = linear.value<nlohmann::json>("delta", nullptr);
                element.is_number())
            delta = element;

        std::optional<std::int64_t> size;
        if (auto element = linear.value<nlohmann::json>("size", nullptr);
                element.is_number())
            size = element;

        return std::make_tuple(start, delta, size);
    }

    return std::make_tuple(std::nullopt, std::nullopt, std::nullopt);
}

std::string wss::struct_field_dimension::name() const
{
    if (_json.contains("name")
            && _json["name"].is_string())
        return _json["name"];

    return "";
}

std::string wss::struct_field_dimension::rule() const
{
    if (_json.contains("rule")
            && _json["rule"].is_string())
        return _json["rule"];

    return rule_types::explicit_rule;
}
