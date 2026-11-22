// SPDX-License-Identifier: MIT
#include <cstddef>

#include <gtest/gtest.h>

#include "ebc/core/delay_line.hpp"

namespace {

TEST(DelayLine, ReturnsPrefillForTheFirstDelayPulses) {
  for (std::size_t delay = 1; delay <= ebc::kMaxMeasurementDelay; ++delay) {
    ebc::DelayLine<double> line{delay};
    line.prefill(4.0);
    for (std::size_t i = 0; i < 50; ++i) {
      const double expected = i < delay ? 4.0 : static_cast<double>(i - delay);
      ASSERT_DOUBLE_EQ(line.read(), expected) << "delay=" << delay << " pulse=" << i;
      line.push(static_cast<double>(i));
    }
  }
}

TEST(DelayLine, PrefillStartsANewBurst) {
  ebc::DelayLine<double> line{2};
  line.prefill(4.0);
  for (int i = 0; i < 7; ++i) line.push(1.0);
  line.prefill(3.0);
  EXPECT_DOUBLE_EQ(line.read(), 3.0);
}

}  // namespace
