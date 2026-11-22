// SPDX-License-Identifier: MIT
// Port of tests/test_controllers.py::TestILR and TestHybrid.
#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "ebc/core/hybrid.hpp"
#include "ebc/core/ilc.hpp"
#include "ebc/core/noise.hpp"

namespace {

using Ilc = ebc::IterativeLearningController<double>;
using Table = Ilc::Table;

double stddev(const Table& t) {
  double mean = 0.0;
  for (const double v : t) mean += v;
  mean /= static_cast<double>(t.size());
  double var = 0.0;
  for (const double v : t) var += (v - mean) * (v - mean);
  return std::sqrt(var / static_cast<double>(t.size()));
}

TEST(Ilc, TableStartsFlat) {
  const Ilc ilc{{}, 4.5};
  for (const double v : ilc.table()) EXPECT_DOUBLE_EQ(v, 4.5);
  EXPECT_EQ(ilc.bursts_learned(), 0U);
}

TEST(Ilc, GridViewIsRowMajor) {
  Ilc ilc{{.learning_rate = 1.0, .q_filter_depth = 1}, 0.0};
  Table errors{};
  errors[37] = 9.0;
  (void)ilc.learn(errors);
  EXPECT_DOUBLE_EQ(ilc.at(3, 7), 9.0);
  EXPECT_DOUBLE_EQ(ilc.command(37), 9.0);
}

TEST(Ilc, UpdateFollowsLearningLawWithoutSmoothing) {
  Ilc ilc{{.learning_rate = 0.5, .q_filter_depth = 1}, 4.0};
  Table errors{};
  errors.fill(2.0);
  EXPECT_DOUBLE_EQ(ilc.learn(errors), 1.0);  // largest correction
  for (const double v : ilc.table()) EXPECT_DOUBLE_EQ(v, 5.0);
}

TEST(Ilc, TableIsClampedAtZero) {
  Ilc ilc{{.learning_rate = 1.0, .q_filter_depth = 1}, 1.0};
  Table errors{};
  errors.fill(-3.0);
  (void)ilc.learn(errors);
  for (const double v : ilc.table()) EXPECT_DOUBLE_EQ(v, 0.0);
}

TEST(Ilc, SmoothingReducesNoiseInTheLearnedProfile) {
  ebc::Xoshiro128pp rng{3};
  Table errors{};
  for (double& e : errors) e = rng.uniform<double>() - 0.5;

  Ilc raw{{.learning_rate = 1.0, .q_filter_depth = 1}, 0.0};
  Ilc filtered{{.learning_rate = 1.0, .q_filter_depth = 5}, 0.0};
  (void)raw.learn(errors);
  (void)filtered.learn(errors);
  EXPECT_LT(stddev(filtered.table()), stddev(raw.table()));
}

TEST(Ilc, ResetRestoresInitialCommand) {
  Ilc ilc{{}, 4.0};
  Table errors{};
  errors.fill(1.0);
  (void)ilc.learn(errors);
  ilc.reset();
  for (const double v : ilc.table()) EXPECT_DOUBLE_EQ(v, 4.0);
  EXPECT_EQ(ilc.bursts_learned(), 0U);
}

TEST(Hybrid, FeedforwardAndFeedbackSumToCommand) {
  ebc::ControllerParams<double> params{};
  params.pid = {.kp = 0.5, .ki = 0.0, .kd = 0.0};
  ebc::HybridController<double> hybrid{params, 4.0};
  const auto a = hybrid.act(5, 4.0, 3.0);
  EXPECT_DOUBLE_EQ(a.feedforward, 4.0);
  EXPECT_DOUBLE_EQ(a.feedback, 0.5);
  EXPECT_DOUBLE_EQ(a.command, 4.5);
}

TEST(Hybrid, CommandIsNeverNegativeAndRespectsLimit) {
  ebc::ControllerParams<double> params{};
  params.pid = {.kp = 5.0, .ki = 0.0, .kd = 0.0, .output_limit = 100.0};
  ebc::HybridController<double> hybrid{params, 0.1};
  EXPECT_DOUBLE_EQ(hybrid.act(0, 4.0, 10.0).command, 0.0);
  EXPECT_DOUBLE_EQ(hybrid.act(1, 4.0, -10.0).command, params.command_limit);
}

}  // namespace
