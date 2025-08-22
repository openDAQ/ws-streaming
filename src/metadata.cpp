#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <boost/multiprecision/cpp_int.hpp>

#include <nlohmann/json.hpp>

#include <ws-streaming/data_types.hpp>
#include <ws-streaming/endianness.hpp>
#include <ws-streaming/metadata.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/detail/json.hpp>

static std::size_t get_primitive_size(const std::string& type)
{
    if (type == wss::data_types::int8_t) return sizeof(std::int8_t);
    if (type == wss::data_types::int16_t) return sizeof(std::int16_t);
    if (type == wss::data_types::int32_t) return sizeof(std::int32_t);
    if (type == wss::data_types::int64_t) return sizeof(std::int64_t);
    if (type == wss::data_types::uint8_t) return sizeof(std::uint8_t);
    if (type == wss::data_types::uint16_t) return sizeof(std::uint16_t);
    if (type == wss::data_types::uint32_t) return sizeof(std::uint32_t);
    if (type == wss::data_types::uint64_t) return sizeof(std::uint64_t);
    if (type == wss::data_types::real32_t) return 4;
    if (type == wss::data_types::real64_t) return 8;

    return 0;
}

wss::metadata::metadata()
    : _json(nlohmann::json::object())
{
}

wss::metadata::metadata(const nlohmann::json& json)
    : _json(json.is_object() ? json : nlohmann::json::object())
{
}

std::string wss::metadata::endian() const
{
    if (auto endian = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/endian"), nullptr);
            endian.is_string())
        return endian;

    return endianness::unknown;
}

std::string wss::metadata::data_type() const
{
    if (auto data_type = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/dataType"), nullptr);
            data_type.is_string())
        return data_type;

    return data_types::unknown_t;
}

std::pair<
    std::optional<std::int64_t>,
    std::optional<std::int64_t>
> wss::metadata::linear_start_delta() const
{
    if (rule() != rule_types::linear_rule)
        return std::make_pair(std::nullopt, std::nullopt);

    if (auto parameters = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/interpretation/rule/parameters"), nullptr);
        parameters.is_object())
    {
        std::optional<std::int64_t> start;
        if (auto element = parameters.value<nlohmann::json>("start", nullptr);
                element.is_number_integer())
            start = element;

        std::optional<std::int64_t> delta;
        if (auto element = parameters.value<nlohmann::json>("delta", nullptr);
                element.is_number_integer())
            delta = element;

        return std::make_pair(start, delta);
    }

    return std::make_pair(std::nullopt, std::nullopt);
}

std::string wss::metadata::name() const
{
    if (auto name = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/name"), nullptr);
            name.is_string())
        return name;

    return "";
}

std::uint64_t wss::metadata::tcp_signal_rate_ticks(
    std::uint64_t numerator,
    std::uint64_t denominator) const
{
    // Direct TCP protocol devices represent time deltas as 96-bit counts of 2^-64 second
    // intervals. But openDAQ works with 64-bit rationals. We will convert this to a count of
    // nanoseconds. Note that binary sample rates cannot be exactly represented in this way. We
    // could support an alternative tick resolution of 2^-30 instead of 10^-9 (nanoseconds) and
    // select the more accurate one. Howver, the direct TCP protocol devices themselves do not
    // represent intervals exactly (!) and it is not possible to reliably determine the intended
    // sample rate in all cases. Therefore we have no choice but to accept small inaccuracies!

    if (!_json.contains("signalRate"))
        return 0;

    const auto& signal_rate = _json.at("signalRate");
    if (!signal_rate.is_object())
        return 0;

    std::uint32_t seconds = detail::json_ptr(signal_rate, "/delta/seconds", std::uint32_t{0});
    std::uint32_t fraction = detail::json_ptr(signal_rate, "/delta/fraction", std::uint32_t{0});
    std::uint32_t sub_fraction = detail::json_ptr(signal_rate, "/delta/subFraction", std::uint32_t{0});
    std::uint32_t samples = detail::json_ptr(signal_rate, "/samples", std::uint32_t{1});

    boost::multiprecision::cpp_int ticks_2_64 = seconds;
    ticks_2_64 <<= 32;
    ticks_2_64 += fraction;
    ticks_2_64 <<= 32;
    ticks_2_64 += sub_fraction;

    boost::multiprecision::cpp_int two_64 = 1;
    two_64 <<= 64;

    // This represents half a quantum, and adding this term below causes the resulting value to be
    // rounded to the nearest integer tick value instead of truncating (rounding down).
    boost::multiprecision::cpp_rational half{
        two_64 * numerator,
        boost::multiprecision::cpp_int{denominator} * 2};

    return static_cast<std::uint64_t>(
        boost::multiprecision::cpp_rational(
            ticks_2_64 * denominator + half * samples,
            two_64 * samples * numerator));
}

std::string wss::metadata::origin() const
{
    if (auto origin = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/origin"), nullptr);
            origin.is_string())
        return origin;

    return "";
}

std::optional<std::pair<double, double>>
wss::metadata::range() const
{
    if (auto range = _json.value<nlohmann::json>(
        nlohmann::json::json_pointer("/definition/range"), nullptr);
        range.is_object())
    {
        double low = 0, high = 0;

        if (range.contains("low") && range["low"].is_number())
            low = range["low"];

        if (range.contains("high") && range["high"].is_number())
            high = range["high"];

        return std::make_pair(low, high);
    }

    return std::nullopt;
}

std::string wss::metadata::rule() const
{
    if (auto rule = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/rule"), nullptr);
            rule.is_string())
        return rule;

    return rule_types::explicit_rule;
}

std::size_t wss::metadata::sample_size() const
{
    std::string type = data_type();

    std::size_t size = get_primitive_size(type);

    if (!size && type == data_types::struct_t)
    {
        if (auto struct_array = _json.value<nlohmann::json>(
            nlohmann::json::json_pointer("/definition/struct"), nullptr);
            struct_array.is_array())
        {
            for (const auto& field : struct_array)
            {
                if (!field.is_object())
                    continue;
                if (!field.contains("dataType") || !field["dataType"].is_string())
                    continue;

                std::size_t field_size = get_primitive_size(field["dataType"]);

                if (auto linear = field.value<nlohmann::json>(
                    nlohmann::json::json_pointer("/dimensions/0/linear"), nullptr);
                    linear.is_object())
                {
                    std::size_t count = 1;
                    if (linear.contains("size") && linear["size"].is_number_integer())
                        count = linear["size"];
                    field_size *= count;
                }

                size += field_size;
            }
        }
    }

    return size;
}

std::string wss::metadata::table_id() const
{
    if (auto table_id = _json.value<nlohmann::json>(
            "tableId", nullptr); table_id.is_string())
        return table_id;

    return "";
}

std::optional<std::pair<std::uint64_t, std::uint64_t>>
wss::metadata::tick_resolution() const
{
    if (auto resolution = _json.value<nlohmann::json>(
        nlohmann::json::json_pointer("/definition/resolution"), nullptr);
        resolution.is_object())
    {
        std::uint64_t numerator = 1, denominator = 1;

        if (resolution.contains("num") && resolution["num"].is_number_integer())
            numerator = resolution["num"];

        if (resolution.contains("denom") && resolution["denom"].is_number_integer())
            denominator = resolution["denom"];

        return std::make_pair(numerator, denominator);
    }

    return std::nullopt;
}

std::optional<wss::unit> wss::metadata::unit() const
{
    if (auto unit = _json.value<nlohmann::json>(
        nlohmann::json::json_pointer("/interpretation/unit"), nullptr);
        unit.is_object())
    {
        int id = -1;
        std::string name, quantity, symbol;

        if (unit.contains("id") && unit["id"].is_number_integer())
            id = unit["id"];

        if (unit.contains("name") && unit["name"].is_string())
            name = unit["name"];

        if (unit.contains("quantity") && unit["quantity"].is_string())
            quantity = unit["quantity"];

        if (unit.contains("symbol") && unit["symbol"].is_string())
            symbol = unit["symbol"];

        return wss::unit(id, name, quantity, symbol);
    }

    return std::nullopt;
}

std::optional<std::int64_t> wss::metadata::value_index() const
{
    if (auto value_index = _json.value<nlohmann::json>(
            "valueIndex", nullptr); value_index.is_number_integer())
        return value_index;

    return std::nullopt;
}

const nlohmann::json& wss::metadata::json() const noexcept
{
    return _json;
}
