#include <cstdint>
#include <optional>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <ws-streaming/metadata.hpp>
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

TEST(MetadataTest, OriginFromAbsoluteReference)
{
    EXPECT_EQ(parse(R"({
        "definition": { "rule": "linear", "absoluteReference": "1970-01-01T00:00:00.0" }
    })").origin(), "1970-01-01T00:00:00.0");
}

TEST(MetadataTest, OriginPrefersOriginOverAbsoluteReference)
{
    EXPECT_EQ(parse(R"({
        "definition": { "rule": "linear", "absoluteReference": "1970-01-01" },
        "interpretation": { "origin": "1970-01-01T00:00:00Z" }
    })").origin(), "1970-01-01T00:00:00Z");
}

TEST(MetadataTest, OriginFromAbsoluteReferenceWhenOriginIsEmpty)
{
    EXPECT_EQ(parse(R"({
        "definition": { "rule": "linear", "origin": "", "absoluteReference": "1970-01-01" },
        "interpretation": { "origin": "" }
    })").origin(), "1970-01-01");
}

TEST(MetadataTest, OriginEmptyAbsoluteReference)
{
    EXPECT_EQ(parse(R"({ "definition": { "rule": "linear", "absoluteReference": "" } })").origin(), "");
}
