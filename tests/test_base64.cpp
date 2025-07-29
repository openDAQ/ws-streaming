#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <ws-streaming/detail/base64.hpp>

using namespace testing;
using namespace wss::detail;

TEST(Base64, CharStrings)
{
    EXPECT_EQ(base64(std::string("")), "");
    EXPECT_EQ(base64(std::string("f")), "Zg==");
    EXPECT_EQ(base64(std::string("fo")), "Zm8=");
    EXPECT_EQ(base64(std::string("foo")), "Zm9v");
    EXPECT_EQ(base64(std::string("foob")), "Zm9vYg==");
    EXPECT_EQ(base64(std::string("fooba")), "Zm9vYmE=");
    EXPECT_EQ(base64(std::string("foobar")), "Zm9vYmFy");
}

TEST(Base64, CharLiterals)
{
    // Note: Here the NULL terminator is included!
    EXPECT_EQ(base64(""), "AA==");
    EXPECT_EQ(base64("f"), "ZgA=");
    EXPECT_EQ(base64("fo"), "Zm8A");
    EXPECT_EQ(base64("foo"), "Zm9vAA==");
    EXPECT_EQ(base64("foob"), "Zm9vYgA=");
    EXPECT_EQ(base64("fooba"), "Zm9vYmEA");
    EXPECT_EQ(base64("foobar"), "Zm9vYmFyAA==");
}

TEST(Base64, CharArrays)
{
    EXPECT_EQ(base64(std::array<char, 0>{ }), "");
    EXPECT_EQ(base64(std::array<char, 1>{ 'f', }), "Zg==");
    EXPECT_EQ(base64(std::array<char, 2>{ 'f', 'o' }), "Zm8=");
    EXPECT_EQ(base64(std::array<char, 3>{ 'f', 'o', 'o' }), "Zm9v");
    EXPECT_EQ(base64(std::array<char, 4>{ 'f', 'o', 'o', 'b' }), "Zm9vYg==");
    EXPECT_EQ(base64(std::array<char, 5>{ 'f', 'o', 'o', 'b', 'a' }), "Zm9vYmE=");
    EXPECT_EQ(base64(std::array<char, 6>{ 'f', 'o', 'o', 'b', 'a', 'r' }), "Zm9vYmFy");
}

TEST(Base64, CharVectors)
{
    EXPECT_EQ(base64(std::vector<char>{ }), "");
    EXPECT_EQ(base64(std::vector<char>{ 'f', }), "Zg==");
    EXPECT_EQ(base64(std::vector<char>{ 'f', 'o' }), "Zm8=");
    EXPECT_EQ(base64(std::vector<char>{ 'f', 'o', 'o' }), "Zm9v");
    EXPECT_EQ(base64(std::vector<char>{ 'f', 'o', 'o', 'b' }), "Zm9vYg==");
    EXPECT_EQ(base64(std::vector<char>{ 'f', 'o', 'o', 'b', 'a' }), "Zm9vYmE=");
    EXPECT_EQ(base64(std::vector<char>{ 'f', 'o', 'o', 'b', 'a', 'r' }), "Zm9vYmFy");
}

TEST(Base64, ByteArrays)
{
    EXPECT_EQ(base64(std::array<std::uint8_t, 0>{ }), "");
    EXPECT_EQ(base64(std::array<std::uint8_t, 1>{ 1 }), "AQ==");
    EXPECT_EQ(base64(std::array<std::uint8_t, 2>{ 1, 2 }), "AQI=");
    EXPECT_EQ(base64(std::array<std::uint8_t, 3>{ 1, 2, 3 }), "AQID");
}

TEST(Base64, ByteVectors)
{
    EXPECT_EQ(base64(std::vector<std::uint8_t>{ }), "");
    EXPECT_EQ(base64(std::vector<std::uint8_t>{ 1 }), "AQ==");
    EXPECT_EQ(base64(std::vector<std::uint8_t>{ 1, 2 }), "AQI=");
    EXPECT_EQ(base64(std::vector<std::uint8_t>{ 1, 2, 3 }), "AQID");
}
