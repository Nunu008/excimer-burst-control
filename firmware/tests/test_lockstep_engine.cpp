// SPDX-License-Identifier: MIT
//
// Port of tests/test_simulation.py. The same control claims the Python suite proves
// must hold for the C++ implementation, in double *and* in single precision (the
// scalar type of the Cortex-R5F / Cortex-M7 builds).
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "ebc/core/noise.hpp"
#include "ebc/sim/lockstep_engine.hpp"

namespace {

using ebc::Strategy;

template <typename T>
class LockstepEngineTest : public ::testing::Test {
 protected:
  using Engine = ebc::sim::LockstepEngine<T>;
  using Snapshot = typename Engine::Snapshot;
  static constexpr std::size_t kPulses = Engine::kPulses;

  /// Runs `bursts` bursts and returns all snapshots (heap is fine in a host test).
  static std::vector<Snapshot> run(const ebc::ControllerParams<T>& params, int bursts,
                                   std::unique_ptr<Engine>* keep = nullptr) {
    auto engine = std::make_unique<Engine>(params);
    ebc::PulseNoise<T> noise{params.plant.noise_sigma_rel, 42};
    std::vector<Snapshot> snapshots(static_cast<std::size_t>(bursts));
    std::array<T, kPulses> factors{};
    for (auto& snapshot : snapshots) {
      for (T& f : factors) f = noise.next();
      engine->step(factors, snapshot);
    }
    if (keep != nullptr) *keep = std::move(engine);
    return snapshots;
  }

  static ebc::ControllerParams<T> noise_free() {
    ebc::ControllerParams<T> params{};
    params.plant.noise_sigma_rel = T{0};
    return params;
  }
};

using Scalars = ::testing::Types<double, float>;
TYPED_TEST_SUITE(LockstepEngineTest, Scalars);

TYPED_TEST(LockstepEngineTest, NoiseFreeOpenLoopFollowsTheEfficiencyProfile) {
  using T = TypeParam;
  const auto params = TestFixture::noise_free();
  std::unique_ptr<typename TestFixture::Engine> engine;
  const auto s = TestFixture::run(params, 1, &engine);
  const auto& open_loop = s[0].result(Strategy::open_loop);
  const T nominal = engine->plant().nominal_command();
  for (const std::size_t i : {0U, 10U, 50U, 99U}) {
    EXPECT_NEAR(open_loop.energy_mj[i], nominal * ebc::gas_efficiency(params.plant, i),
                ebc::real<T>(1e-5));
  }
}

TYPED_TEST(LockstepEngineTest, DeadTimeHoldsFeedbackOffForTheFirstPulses) {
  using T = TypeParam;
  auto params = TestFixture::noise_free();
  params.measurement_delay_pulses = 3;
  std::unique_ptr<typename TestFixture::Engine> engine;
  const auto s = TestFixture::run(params, 1, &engine);
  for (std::size_t i = 0; i < 3; ++i) {
    EXPECT_NEAR(s[0].result(Strategy::pid_only).command[i], engine->plant().nominal_command(),
                ebc::real<T>(1e-6));
    EXPECT_NEAR(s[0].result(Strategy::hybrid_ilc_pid).feedback[i], T{0}, ebc::real<T>(1e-6));
  }
}

TYPED_TEST(LockstepEngineTest, PidEnergyStaysBoundedOverManyBursts) {
  using T = TypeParam;
  for (const auto& s : TestFixture::run({}, 20)) {
    for (const T e : s.result(Strategy::pid_only).energy_mj) {
      ASSERT_LT(e, T{8});
      ASSERT_GT(e, T{0});
    }
  }
}

TYPED_TEST(LockstepEngineTest, HybridLearningConvergesAndBeatsOpenLoop) {
  using T = TypeParam;
  const auto s = TestFixture::run({}, 15);
  const auto& last = s.back();
  const T first_rms = s.front().result(Strategy::hybrid_ilc_pid).kpis.rms_mj;
  const T last_rms = last.result(Strategy::hybrid_ilc_pid).kpis.rms_mj;
  EXPECT_LT(last_rms, ebc::real<T>(0.08));
  EXPECT_LT(last_rms, ebc::real<T>(0.4) * first_rms);
  EXPECT_LT(last_rms, ebc::real<T>(0.4) * last.result(Strategy::open_loop).kpis.rms_mj);
}

TYPED_TEST(LockstepEngineTest, HybridBeatsPurePidAfterConvergence) {
  using T = TypeParam;
  const auto runs = TestFixture::run({}, 15);
  const auto& last = runs.back();
  const auto& hybrid = last.result(Strategy::hybrid_ilc_pid).kpis;
  const auto& pid = last.result(Strategy::pid_only).kpis;
  EXPECT_LT(hybrid.rms_mj, ebc::real<T>(0.5) * pid.rms_mj);
  EXPECT_LT(hybrid.max_deviation_pct, pid.max_deviation_pct);
}

TYPED_TEST(LockstepEngineTest, LearnedProfileApproachesInverseEfficiency) {
  using T = TypeParam;
  const auto params = TestFixture::noise_free();
  std::unique_ptr<typename TestFixture::Engine> engine;
  (void)TestFixture::run(params, 25, &engine);
  for (const std::size_t i : {0U, 1U, 25U, 60U, 99U}) {
    const T expected = params.plant.target_energy_mj / ebc::gas_efficiency(params.plant, i);
    EXPECT_NEAR(engine->learning().command(i), expected, ebc::real<T>(0.01) * expected) << "pulse " << i;
  }
}

TYPED_TEST(LockstepEngineTest, FeedbackShareDecaysWhileTheTableTakesOver) {
  using T = TypeParam;
  const auto s = TestFixture::run({}, 15);
  const T first = s.front().result(Strategy::hybrid_ilc_pid).mean_abs_feedback();
  const T last = s.back().result(Strategy::hybrid_ilc_pid).mean_abs_feedback();
  EXPECT_LT(last, ebc::real<T>(0.2) * first);
  EXPECT_LT(last, ebc::real<T>(0.05));
}

TYPED_TEST(LockstepEngineTest, LearningIsMonotoneInTheFirstBursts) {
  const auto s = TestFixture::run({}, 8);
  const auto rms = [&](std::size_t k) { return s[k].result(Strategy::hybrid_ilc_pid).kpis.rms_mj; };
  EXPECT_LT(rms(7), rms(3));
  EXPECT_LT(rms(3), rms(1));
}

TYPED_TEST(LockstepEngineTest, LatencyPresetKeepsEverythingStable) {
  using T = TypeParam;
  const auto runs = TestFixture::run(ebc::make_preset<T>(ebc::Preset::high_latency), 10);
  const auto& last = runs.back();
  EXPECT_LT(last.result(Strategy::hybrid_ilc_pid).kpis.rms_mj, ebc::real<T>(0.2));
  for (const T e : last.result(Strategy::pid_only).energy_mj) EXPECT_TRUE(std::isfinite(e));
}

TYPED_TEST(LockstepEngineTest, NoisePresetDegradesPidMoreThanHybrid) {
  using T = TypeParam;
  const auto runs = TestFixture::run(ebc::make_preset<T>(ebc::Preset::strong_gas_noise), 12);
  const auto& last = runs.back();
  EXPECT_LT(last.result(Strategy::hybrid_ilc_pid).kpis.rms_mj,
            ebc::real<T>(0.6) * last.result(Strategy::pid_only).kpis.rms_mj);
}

}  // namespace
