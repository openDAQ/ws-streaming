#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <ws-streaming/data_types.hpp>
#include <ws-streaming/dimension_builder.hpp>
#include <ws-streaming/metadata.hpp>
#include <ws-streaming/metadata_builder.hpp>
#include <ws-streaming/rule_types.hpp>
#include <ws-streaming/unit.hpp>

using namespace testing;

namespace {

wss::metadata parse(const char *json)
{
    return wss::metadata(nlohmann::json::parse(json));
}

} // namespace

TEST(MetadataTest, LinearStartDeltaFromInterpretation)
{
    auto [start, delta] = parse(R"({
        "definition": { "rule": "linear" },
        "interpretation": { "rule": { "type": 1, "parameters": { "start": 100, "delta": 7 } } }
    })").linear_start_delta();

    EXPECT_EQ(start, std::optional<std::int64_t>(100));
    EXPECT_EQ(delta, std::optional<std::int64_t>(7));
}

TEST(MetadataTest, LinearStartDeltaFromDefinition)
{
    auto [start, delta] = parse(R"({
        "definition": { "rule": "linear", "linear": { "start": 200, "delta": 9 } }
    })").linear_start_delta();

    EXPECT_EQ(start, std::optional<std::int64_t>(200));
    EXPECT_EQ(delta, std::optional<std::int64_t>(9));
}

TEST(MetadataTest, LinearStartDeltaPrefersInterpretation)
{
    auto [start, delta] = parse(R"({
        "definition": { "rule": "linear", "linear": { "start": 200, "delta": 9 } },
        "interpretation": { "rule": { "type": 1, "parameters": { "start": 100, "delta": 7 } } }
    })").linear_start_delta();

    EXPECT_EQ(start, std::optional<std::int64_t>(100));
    EXPECT_EQ(delta, std::optional<std::int64_t>(7));
}

TEST(MetadataTest, LinearStartDeltaIgnoresNonLinearRules)
{
    auto [start, delta] = parse(R"({
        "definition": { "rule": "explicit", "linear": { "start": 200, "delta": 9 } },
        "interpretation": { "rule": { "type": 1, "parameters": { "start": 100, "delta": 7 } } }
    })").linear_start_delta();

    EXPECT_EQ(start, std::nullopt);
    EXPECT_EQ(delta, std::nullopt);
}

TEST(MetadataTest, LinearStartDeltaAbsent)
{
    auto [start, delta] = parse(R"({ "definition": { "rule": "linear" } })").linear_start_delta();

    EXPECT_EQ(start, std::nullopt);
    EXPECT_EQ(delta, std::nullopt);
}

TEST(MetadataTest, LinearStartDeltaIgnoresNonNumericParameters)
{
    auto [start, delta] = parse(R"({
        "definition": { "rule": "linear" },
        "interpretation": { "rule": { "type": 1, "parameters": { "start": "x", "delta": null } } }
    })").linear_start_delta();

    EXPECT_EQ(start, std::nullopt);
    EXPECT_EQ(delta, std::nullopt);
}

TEST(MetadataTest, UnitFromInterpretation)
{
    auto unit = parse(R"({
        "interpretation": { "unit": {
            "id": 5, "name": "volts", "quantity": "voltage", "symbol": "V" } }
    })").unit();

    ASSERT_TRUE(unit.has_value());
    EXPECT_EQ(unit->id(), 5);
    EXPECT_EQ(unit->name(), "volts");
    EXPECT_EQ(unit->quantity(), "voltage");
    EXPECT_EQ(unit->symbol(), "V");
}

TEST(MetadataTest, UnitFromDefinition)
{
    auto unit = parse(R"({
        "definition": { "unit": {
            "unitId": 5, "displayName": "V", "quantity": "voltage" } }
    })").unit();

    ASSERT_TRUE(unit.has_value());
    EXPECT_EQ(unit->id(), 5);
    EXPECT_EQ(unit->quantity(), "voltage");
    EXPECT_EQ(unit->symbol(), "V");

    // "definition" has no separate name, so the display name serves as both.
    EXPECT_EQ(unit->name(), "V");
}

TEST(MetadataTest, UnitPrefersInterpretation)
{
    auto unit = parse(R"({
        "definition": { "unit": { "unitId": 9, "displayName": "A", "quantity": "current" } },
        "interpretation": { "unit": {
            "id": 5, "name": "volts", "quantity": "voltage", "symbol": "V" } }
    })").unit();

    ASSERT_TRUE(unit.has_value());
    EXPECT_EQ(unit->id(), 5);
    EXPECT_EQ(unit->symbol(), "V");
}

TEST(MetadataTest, UnitAbsent)
{
    EXPECT_EQ(parse(R"({ "definition": { "rule": "explicit" } })").unit(), std::nullopt);
}

TEST(MetadataTest, ScalarSampleSize)
{
    EXPECT_EQ(parse(R"({ "definition": { "dataType": "real64" } })").sample_size(), 8u);
    EXPECT_TRUE(parse(R"({ "definition": { "dataType": "real64" } })").dimensions().empty());
}

TEST(MetadataTest, SampleSizeIncludesLinearDimensions)
{
    // A spectrum: each sample is a vector of 1024 real64 values
    auto metadata = parse(R"({
        "definition": {
            "dataType": "real64",
            "rule": "explicit",
            "dimensions": [
                { "name": "frequency", "rule": "linear", "linear": { "start": 0.0, "delta": 10.0, "size": 1024 } }
            ]
        }
    })");

    EXPECT_EQ(metadata.sample_size(), 8u * 1024);
    ASSERT_EQ(metadata.dimensions().size(), 1u);
    EXPECT_EQ(metadata.dimensions()[0].name(), "frequency");
}

TEST(MetadataTest, SampleSizeMultipliesAllDimensions)
{
    EXPECT_EQ(parse(R"({
        "definition": {
            "dataType": "int16",
            "dimensions": [
                { "name": "x", "rule": "linear", "linear": { "start": 0, "delta": 1, "size": 3 } },
                { "name": "y", "rule": "linear", "linear": { "start": 0, "delta": 1, "size": 4 } }
            ]
        }
    })").sample_size(), 2u * 3 * 4);
}

TEST(MetadataTest, SampleSizeIgnoresDimensionsWithoutSize)
{
    EXPECT_EQ(parse(R"({
        "definition": {
            "dataType": "real32",
            "dimensions": [ { "name": "x", "rule": "linear", "linear": { "start": 0, "delta": 1 } } ]
        }
    })").sample_size(), 4u);
}

TEST(MetadataBuilderTest, Dimension)
{
    wss::metadata metadata = wss::metadata_builder("spectrum")
        .data_type(wss::data_types::real64_t)
        .dimension(wss::dimension_builder("frequency").linear_rule(0, 25, 512))
        .build();

    EXPECT_EQ(metadata.sample_size(), 8u * 512);
    ASSERT_EQ(metadata.dimensions().size(), 1u);

    auto [start, delta, size] = metadata.dimensions()[0].linear_start_delta_size();
    EXPECT_EQ(start, std::optional<std::int64_t>(0));
    EXPECT_EQ(delta, std::optional<std::int64_t>(25));
    EXPECT_EQ(size, std::optional<std::int64_t>(512));
}

TEST(MetadataBuilderTest, DimensionWithRealLinearRule)
{
    wss::metadata metadata = wss::metadata_builder("spectrum")
        .data_type(wss::data_types::real64_t)
        .dimension(wss::dimension_builder("frequency").linear_rule_real(0.5, 51.2, 500))
        .build();

    EXPECT_EQ(metadata.sample_size(), 8u * 500);
    const auto dims = metadata.dimensions();
    ASSERT_EQ(dims.size(), 1u);

    const auto& dim = dims[0];
    EXPECT_EQ(dim.rule(), wss::rule_types::linear_rule);
    EXPECT_DOUBLE_EQ(dim.json()["linear"]["start"].get<double>(), 0.5);
    EXPECT_DOUBLE_EQ(dim.json()["linear"]["delta"].get<double>(), 51.2);
    EXPECT_EQ(std::get<2>(dim.linear_start_delta_size()), std::optional<std::int64_t>(500));
}

TEST(MetadataBuilderTest, MultipleDimensions)
{
    wss::metadata metadata = wss::metadata_builder("matrix")
        .data_type(wss::data_types::uint8_t)
        .dimension(wss::dimension_builder("rows").linear_rule(0, 1, 2))
        .dimension(wss::dimension_builder("columns").linear_rule(0, 1, 5))
        .build();

    ASSERT_EQ(metadata.dimensions().size(), 2u);
    EXPECT_EQ(metadata.dimensions()[0].name(), "rows");
    EXPECT_EQ(metadata.dimensions()[1].name(), "columns");
    EXPECT_EQ(metadata.sample_size(), 10u);
}
