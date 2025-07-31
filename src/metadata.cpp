#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include <ws-streaming/metadata.hpp>

wss::metadata::metadata(const nlohmann::json& json)
    : _json(json.is_object() ? json : nlohmann::json::object())
{
}

std::string wss::metadata::data_type() const
{
    if (auto data_type = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/dataType"), nullptr);
            data_type.is_string())
        return data_type;

    return "";
}

bool wss::metadata::is_linear_rule() const
{
    if (auto rule = _json.value<nlohmann::json>(
                nlohmann::json::json_pointer("/definition/rule"), nullptr);
            rule.is_string() && rule == "linear")
        return true;

    return false;
}

std::pair<std::int64_t, std::int64_t> wss::metadata::linear_start_delta() const
{
    if (!is_linear_rule())
        return std::make_pair(0, 0);

    if (auto parameters = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/interpretation/rule/parameters"), nullptr);
        parameters.is_object())
    {
        std::int64_t start = 0;
        if (auto element = parameters.value<nlohmann::json>("start", nullptr);
                element.is_number_integer())
            start = element;

        std::int64_t delta = 0;
        if (auto element = parameters.value<nlohmann::json>("delta", nullptr);
                element.is_number_integer())
            delta = element;

        return std::make_pair(start, delta);
    }

    return std::make_pair(0, 0);
}

std::string wss::metadata::table_id() const
{
    if (auto table_id = _json.value<nlohmann::json>(
            "tableId", nullptr); table_id.is_string())
        return table_id;

    return "";
}

const nlohmann::json& wss::metadata::json() const noexcept
{
    return _json;
}
