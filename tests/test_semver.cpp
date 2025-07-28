#include <optional>

#include <gtest/gtest.h>

#include <ws-streaming/detail/semver.hpp>

using namespace testing;
using namespace wss::detail;

TEST(SemVerTest, DefaultConstructor)
{
    semver ver;

    EXPECT_EQ(ver.major(), 0);
    EXPECT_EQ(ver.minor(), 0);
    EXPECT_EQ(ver.revision(), 0);
}

TEST(SemVerTest, ExplicitConstructor)
{
    semver ver(1, 2, 3);

    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.revision(), 3);
}

TEST(SemVerTest, TryParse)
{
    EXPECT_EQ(semver::try_parse(""), std::nullopt);
    EXPECT_EQ(semver::try_parse("1"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1.2"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1.2.3x"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1.2.x3"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1.2x.3"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1.x2.3"), std::nullopt);
    EXPECT_EQ(semver::try_parse("1x.2.3"), std::nullopt);
    EXPECT_EQ(semver::try_parse("x1.2.3"), std::nullopt);

    EXPECT_EQ(semver::try_parse("1.2.3"), semver(1, 2, 3));
}

TEST(SemVerTest, Compare)
{
    EXPECT_GE(semver(2, 0, 0), semver(1, 0, 0));
    EXPECT_GE(semver(2, 2, 0), semver(2, 1, 0));
    EXPECT_GE(semver(2, 2, 2), semver(2, 2, 1));

    EXPECT_LE(semver(1, 0, 0), semver(2, 0, 0));
    EXPECT_LE(semver(2, 1, 0), semver(2, 2, 0));
    EXPECT_LE(semver(2, 2, 1), semver(2, 2, 2));

    EXPECT_NE(semver(1, 1, 1), semver(1, 1, 2));
    EXPECT_NE(semver(1, 1, 1), semver(1, 2, 1));
    EXPECT_NE(semver(1, 1, 1), semver(2, 1, 1));
}
