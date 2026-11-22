// SPDX-License-Identifier: MIT
// Port of tests/test_controllers.py::TestPID.
#include <gtest/gtest.h>

#include "ebc/core/pid.hpp"

namespace {

using Pid = ebc::Pid<double>;
using Params = ebc::PidParams<double>;

TEST(Pid, ZeroErrorProducesNoAction) {
  Pid pid{Params{}};
  EXPECT_DOUBLE_EQ(pid.update(0.0), 0.0);
}

TEST(Pid, ProportionalTermDominatesForFirstSample) {
  Pid pid{{.kp = 0.5, .ki = 0.0, .kd = 0.0}};
  EXPECT_DOUBLE_EQ(pid.update(2.0), 1.0);
}

TEST(Pid, IntegralIsClampedToAntiWindupLimit) {
  Pid pid{{.kp = 0.0, .ki = 1.0, .kd = 0.0, .integral_limit = 5.0}};
  for (int i = 0; i < 100; ++i) (void)pid.update(1.0);
  EXPECT_DOUBLE_EQ(pid.integral(), 5.0);
}

TEST(Pid, OutputIsClamped) {
  Pid pid{{.kp = 100.0, .ki = 0.0, .kd = 0.0, .output_limit = 3.0}};
  EXPECT_DOUBLE_EQ(pid.update(10.0), 3.0);
  EXPECT_DOUBLE_EQ(pid.update(-10.0), -3.0);
}

TEST(Pid, NegativeErrorReducesCommand) {
  Pid pid{{.kp = 0.5, .ki = 0.0, .kd = 0.0}};
  const auto a = pid.act(4.0, 4.4, 4.0);
  EXPECT_LT(a.command, 4.0);
  EXPECT_LT(a.feedback, 0.0);
  EXPECT_DOUBLE_EQ(a.feedforward, 4.0);
}

TEST(Pid, CommandIsNeverNegative) {
  Pid pid{{.kp = 5.0, .ki = 0.0, .kd = 0.0}};
  EXPECT_DOUBLE_EQ(pid.act(4.0, 10.0, 0.1).command, 0.0);
}

TEST(Pid, DerivativeActsOnErrorChange) {
  Pid pid{{.kp = 0.0, .ki = 0.0, .kd = 1.0}};
  EXPECT_DOUBLE_EQ(pid.update(1.0), 1.0);
  EXPECT_DOUBLE_EQ(pid.update(1.0), 0.0);
  EXPECT_DOUBLE_EQ(pid.update(0.5), -0.5);
}

TEST(Pid, ResetClearsState) {
  Pid pid{Params{}};
  for (int i = 0; i < 5; ++i) (void)pid.update(1.0);
  pid.reset();
  EXPECT_DOUBLE_EQ(pid.integral(), 0.0);
  EXPECT_DOUBLE_EQ(pid.update(0.0), 0.0);
}

}  // namespace
