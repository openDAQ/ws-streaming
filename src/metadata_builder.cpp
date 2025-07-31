#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include <ws-streaming/metadata_builder.hpp>

wss::metadata_builder::metadata_builder(
        const std::string& id,
        const std::string& name)
    : _metadata(
        {
            { "definition", {
                { "name", name },
                { "dataType", "real64" },
                { "rule", "explicit" }
            } },
            { "interpretation", {
                { "sig_name", name },
                { "sig_desc", "" },
                { "desc_name", name },
                { "rule", {
                    { "type", 3 },
                    { "parameters", nullptr }
                } }
            } },
            { "tableId", id },
            { "valueIndex", 0 }
        })
{
}

wss::metadata_builder::metadata_builder(
        const nlohmann::json& metadata)
    : _metadata(metadata)
{
}

wss::metadata_builder& wss::metadata_builder::data_type(
    const std::string& type)
{
    _metadata["definition"]["dataType"] = type;

    return *this;
}

wss::metadata_builder& wss::metadata_builder::linear_rule(
    std::int64_t start,
    std::int64_t delta)
{
    _metadata["definition"]["rule"] = "linear";
    _metadata["definition"]["linear"] = { { "delta", delta } };

    if (start)
        _metadata["definition"]["linear"]["start"] = start;

    _metadata["interpretation"]["rule"] = {
        { "type", 1 },
        { "parameters", {
            { "delta", delta },
            { "start", start }
        } }
    };

    return *this;
}

wss::metadata_builder& wss::metadata_builder::origin(
    const std::string& origin)
{
    _metadata["interpretation"]["origin"] = origin;

    return *this;
}

wss::metadata_builder& wss::metadata_builder::table(
    const std::string& id)
{
    _metadata["tableId"] = id;

    return *this;
}

wss::metadata_builder& wss::metadata_builder::unit(
    int id,
    const std::string& name,
    const std::string& quantity,
    const std::string& symbol)
{
    _metadata["definition"]["unit"] = {
        { "unitId", id },
        { "quantity", quantity },
        { "displayName", symbol },
    };

    _metadata["interpretation"]["unit"] = {
        { "id", id },
        { "name", name },
        { "quantity", quantity },
        { "symbol", symbol },
    };

    return *this;
}

wss::metadata_builder& wss::metadata_builder::range(
    double low,
    double high)
{
    _metadata["definition"]["range"] = {
        { "low", low },
        { "high", high }
    };

    return *this;
}

wss::metadata_builder& wss::metadata_builder::resolution(
    std::uint64_t numerator,
    std::uint64_t denominator)
{
    _metadata["definition"]["resolution"] = {
        { "num", numerator },
        { "denom", denominator }
    };

    return *this;
}

const nlohmann::json& wss::metadata_builder::build() const noexcept
{
    return _metadata;
}
