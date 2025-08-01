#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/struct_field_builder.hpp>

wss::struct_field_builder::struct_field_builder(
        const std::string& name)
    : _field(
        {
            { "name", name },
            { "rule", "explicit "}
        })
{
}

wss::struct_field_builder& wss::struct_field_builder::array(
    std::size_t size)
{
    _field["dimensions"] = {
        {
            { "name", "Dimension" },
            { "rule", "linear" },
            { "linear", {
                { "delta", 0 },
                { "size", size },
                { "start", 1 }
            } },
        }
    };

    return *this;
}

wss::struct_field_builder& wss::struct_field_builder::data_type(
    const std::string& type)
{
    _field["dataType"] = type;

    return *this;
}

const nlohmann::json& wss::struct_field_builder::build() const noexcept
{
    return _field;
}
