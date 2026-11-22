// SPDX-License-Identifier: MIT
#include <cmath>

#include <gtest/gtest.h>

#include "ebc/core/noise.hpp"

namespace {

TEST(Noise, ZeroSigmaIsNoiseFree) {
  ebc::PulseNoise<float> noise{0.0F, 1};
  for (int i = 0; i < 100; ++i) EXPECT_FLOAT_EQ(noise.next(), 1.0F);
}

TEST(Noise, MomentsMatchTheRequestedSigma) {
  constexpr double kSigma = 0.01;
  ebc::PulseNoise<double> noise{kSigma, 42};
  constexpr int kSamples = 200'000;
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int i = 0; i < kSamples; ++i) {
    const double x = noise.next();
    sum += x;
    sum_sq += x * x;
  }
  const double mean = sum / kSamples;
  const double sigma = std::sqrt(sum_sq / kSamples - mean * mean);
  EXPECT_NEAR(mean, 1.0, 1e-4);
  EXPECT_NEAR(sigma, kSigma, 0.02 * kSigma);
}

TEST(Noise, IsClippedAtTwentyPercentLikeThePythonPlant) {
  ebc::PulseNoise<double> noise{1.0, 7};  // absurd sigma to hit the clip
  for (int i = 0; i < 10'000; ++i) EXPECT_GE(noise.next(), 0.2);
}

TEST(Noise, SameSeedSameSequence) {
  ebc::PulseNoise<float> a{0.02F, 99};
  ebc::PulseNoise<float> b{0.02F, 99};
  for (int i = 0; i < 1000; ++i) ASSERT_EQ(a.next(), b.next());
}

}  // namespace
