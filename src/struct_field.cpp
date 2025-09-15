#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/data_types.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/struct_field.hpp>

wss::struct_field::struct_field()
    : _json(nlohmann::json::object())
{
}

wss::struct_field::struct_field(const nlohmann::json& json)
    : _json(json.is_object() ? json : nlohmann::json::object())
{
    std::cout << "struct_field constructed with " << json.dump() << std::endl;
}

std::string wss::struct_field::data_type() const
{
    if (_json.contains("dataType")
            && _json["dataType"].is_string())
        return _json["dataType"];

    return data_types::unknown_t;
}

std::string wss::struct_field::name() const
{
    if (_json.contains("name")
            && _json["name"].is_string())
        return _json["name"];

    return "";
}

std::string wss::struct_field::rule() const
{
    if (_json.contains("rule")
            && _json["rule"].is_string())
        return _json["rule"];

    return wss::rule_types::explicit_rule;
}
