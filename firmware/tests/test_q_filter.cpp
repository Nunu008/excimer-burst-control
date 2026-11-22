// SPDX-License-Identifier: MIT
#include <array>

#include <gtest/gtest.h>

#include "ebc/core/q_filter.hpp"

namespace {

TEST(QFilter, DepthOneIsAnUnmodifiedCopy) {
  const std::array<double, 5> in{1, -2, 3, -4, 5};
  std::array<double, 5> out{};
  ebc::centred_moving_average<double, 5>(in, out, 1);
  EXPECT_EQ(out, in);
}

TEST(QFilter, OddDepthUsesEdgePadding) {
  // numpy: padded [1,1,2,3,4,4], window 3
  const std::array<double, 4> in{1, 2, 3, 4};
  std::array<double, 4> out{};
  ebc::centred_moving_average<double, 4>(in, out, 3);
  EXPECT_DOUBLE_EQ(out[0], 4.0 / 3.0);
  EXPECT_DOUBLE_EQ(out[1], 2.0);
  EXPECT_DOUBLE_EQ(out[2], 3.0);
  EXPECT_DOUBLE_EQ(out[3], 11.0 / 3.0);
}

TEST(QFilter, DepthLargerThanSignalIsClampedToSignalLength) {
  const std::array<double, 3> in{3, 6, 9};
  std::array<double, 3> clamped{};
  std::array<double, 3> exact{};
  ebc::centred_moving_average<double, 3>(in, clamped, 50);
  ebc::centred_moving_average<double, 3>(in, exact, 3);
  EXPECT_EQ(clamped, exact);
}

TEST(QFilter, ConstantSignalIsAFixedPoint) {
  std::array<float, 100> in{};
  in.fill(0.25F);
  std::array<float, 100> out{};
  ebc::centred_moving_average<float, 100>(in, out, 5);
  for (const float v : out) EXPECT_FLOAT_EQ(v, 0.25F);
}

}  // namespace
