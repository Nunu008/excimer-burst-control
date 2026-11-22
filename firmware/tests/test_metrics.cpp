// SPDX-License-Identifier: MIT
// Port of tests/test_metrics.py.
#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "ebc/core/metrics.hpp"

namespace {

constexpr double kTarget = 4.0;
using Burst = std::array<double, 100>;

TEST(Metrics, PerfectTrackingYieldsZeroError) {
  Burst e{};
  e.fill(kTarget);
  const auto k = ebc::compute_kpis<double, 100>(e, kTarget, 0.02);
  EXPECT_DOUBLE_EQ(k.max_deviation_pct, 0.0);
  EXPECT_DOUBLE_EQ(k.std_mj, 0.0);
  EXPECT_DOUBLE_EQ(k.rms_mj, 0.0);
  ASSERT_TRUE(k.settling_pulse.has_value());
  EXPECT_EQ(*k.settling_pulse, 0U);
}

TEST(Metrics, ConstantOffset) {
  Burst e{};
  e.fill(kTarget + 0.2);
  const auto k = ebc::compute_kpis<double, 100>(e, kTarget, 0.02);
  EXPECT_NEAR(k.max_deviation_pct, 5.0, 1e-12);
  EXPECT_NEAR(k.std_mj, 0.0, 1e-12);
  EXPECT_NEAR(k.rms_mj, 0.2, 1e-12);
  EXPECT_FALSE(k.settled());
}

TEST(Metrics, DecayingErrorSettlesInsideTheBand) {
  Burst e{};
  for (std::size_t i = 0; i < e.size(); ++i) {
    e[i] = kTarget + 0.8 * std::exp(-static_cast<double>(i) / 4.0);
  }
  const auto k = ebc::compute_kpis<double, 100>(e, kTarget, 0.02);
  EXPECT_NEAR(k.max_deviation_pct, 20.0, 1e-12);
  ASSERT_TRUE(k.settled());
  EXPECT_GE(*k.settling_pulse, 10U);
  EXPECT_LE(*k.settling_pulse, 40U);
}

TEST(Metrics, MonotonicDriftNeverSettles) {
  Burst e{};
  for (std::size_t i = 0; i < e.size(); ++i) {
    e[i] = kTarget - 0.3 * kTarget * static_cast<double>(i) / 99.0;
  }
  const auto k = ebc::compute_kpis<double, 100>(e, kTarget, 0.02);
  EXPECT_FALSE(k.settled());
  EXPECT_NEAR(k.max_deviation_pct, 30.0, 1e-9);
}

TEST(Metrics, SettlingIgnoresIsolatedNoiseSpike) {
  std::array<double, 50> errors{};
  errors[3] = 0.5;
  const auto s = ebc::settling_pulse<double, 50>(errors, 0.08);
  ASSERT_TRUE(s.has_value());
  EXPECT_EQ(*s, 6U);
}

TEST(Metrics, RmsMatchesDefinition) {
  const std::array<double, 3> e{3.0, 4.0, 5.0};
  const auto k = ebc::compute_kpis<double, 3>(e, kTarget, 0.02);
  EXPECT_DOUBLE_EQ(k.rms_mj, std::sqrt(2.0 / 3.0));
}

}  // namespace
