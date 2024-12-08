#include "ntt/ntt.hpp"

#include <gtest/gtest.h>

#include <limits>

class UtilTest : public testing::Test {
protected:
  long v;
  unsigned short u_short;
};

TEST_F(UtilTest, ParseLong) {
  int rc = ntt_long_from_cstr(&v, "1234");
  ASSERT_EQ(rc, 1);
  ASSERT_EQ(v, 1234);
}

TEST_F(UtilTest, ParseNegLong) {
  int rc = ntt_long_from_cstr(&v, "-1234");
  ASSERT_EQ(rc, 1);
  ASSERT_EQ(v, -1234);
}

TEST_F(UtilTest, EmptyCStr) {
  int rc = ntt_long_from_cstr(&v, "");
  ASSERT_EQ(rc, 0);
}

TEST_F(UtilTest, Overflow) {
  auto str = std::to_string(std::numeric_limits<long>::max()) + '0';
  int rc = ntt_long_from_cstr(&v, str.c_str());
  ASSERT_EQ(rc, 0);
}

TEST_F(UtilTest, ParseUnsignedShort) {
  int rc = ntt_unsigned_short_from_cstr(&u_short, "1234");
  ASSERT_EQ(rc, 1);
  ASSERT_EQ(u_short, 1234);
}

TEST_F(UtilTest, ParseUnsignedShortNegValue) {
  int rc = ntt_unsigned_short_from_cstr(&u_short, "-1234");
  ASSERT_EQ(rc, 0);
}

TEST_F(UtilTest, ParseUnsignedShortEmptyCStr) {
  int rc = ntt_unsigned_short_from_cstr(&u_short, "");
  ASSERT_EQ(rc, 0);
}

TEST_F(UtilTest, ParseUnsignedShortOverflow) {
  auto str = std::to_string(std::numeric_limits<unsigned short>::max()) + '0';
  int rc = ntt_unsigned_short_from_cstr(&u_short, str.c_str());
  ASSERT_EQ(rc, 0);
}

TEST_F(UtilTest, ParseUnsignedShortDirty) {
  int rc = ntt_unsigned_short_from_cstr(&u_short, "1234asdf");
  ASSERT_EQ(rc, 0);
}