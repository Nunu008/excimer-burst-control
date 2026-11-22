// SPDX-License-Identifier: MIT
//
// The interrupt-driven controller must be a faithful re-arrangement of the
// simulation: if learning keeps up, it produces the same energies as the
// lock-step hybrid; if it falls behind, it drops bursts instead of blocking.
#include <array>
#include <memory>

#include <gtest/gtest.h>

#include "ebc/core/delay_line.hpp"
#include "ebc/core/noise.hpp"
#include "ebc/core/realtime_controller.hpp"
#include "ebc/sim/lockstep_engine.hpp"

namespace {

using ebc::Strategy;
constexpr std::size_t kPulses = ebc::DefaultGeometry::pulses;
using Burst = std::array<double, kPulses>;

/// Drives one burst through the real-time controller the way the R5F interrupt
/// handler does: begin, then per pulse compute -> emit -> record, then end.
Burst run_burst(ebc::RealtimeController<double>& ctl, const ebc::ExcimerPlantModel<double>& plant,
                const ebc::ControllerParams<double>& params, const Burst& noise) {
  Burst energy{};
  ebc::DelayLine<double> timelog{params.measurement_delay_pulses};
  timelog.prefill(params.plant.target_energy_mj);
  ctl.begin_burst();
  for (std::size_t i = 0; i < kPulses; ++i) {
    const auto a = ctl.on_pulse(i, timelog.read());
    energy[i] = plant.emit(a.command, i, noise[i]);
    ctl.record(i, energy[i]);
    timelog.push(energy[i]);
  }
  ctl.end_burst();
  return energy;
}

TEST(RealtimeController, MatchesTheLockstepHybridWhenLearningKeepsUp) {
  const ebc::ControllerParams<double> params{};
  const ebc::ExcimerPlantModel<double> plant{params.plant};
  ebc::RealtimeController<double> ctl{params, plant.nominal_command(), Strategy::hybrid_ilc_pid};
  auto engine = std::make_unique<ebc::sim::LockstepEngine<double>>(params);
  auto snapshot = std::make_unique<ebc::sim::LockstepEngine<double>::Snapshot>();

  ebc::PulseNoise<double> noise{params.plant.noise_sigma_rel, 5};
  for (int burst = 0; burst < 20; ++burst) {
    Burst factors{};
    for (double& f : factors) f = noise.next();
    const Burst energy = run_burst(ctl, plant, params, factors);
    ASSERT_TRUE(ctl.service_learning());
    engine->step(factors, *snapshot);
    const auto& expected = snapshot->result(Strategy::hybrid_ilc_pid).energy_mj;
    for (std::size_t i = 0; i < kPulses; ++i) {
      ASSERT_DOUBLE_EQ(energy[i], expected[i]) << "burst " << burst << " pulse " << i;
    }
  }
  EXPECT_EQ(ctl.bursts_learned(), 20U);
  EXPECT_EQ(ctl.learning_overruns(), 0U);
}

TEST(RealtimeController, DropsBurstsInsteadOfBlockingWhenLearningFallsBehind) {
  const ebc::ControllerParams<double> params{};
  const ebc::ExcimerPlantModel<double> plant{params.plant};
  ebc::RealtimeController<double> ctl{params, plant.nominal_command(), Strategy::hybrid_ilc_pid};
  Burst ones{};
  ones.fill(1.0);

  (void)run_burst(ctl, plant, params, ones);  // hand-over 1 pending
  (void)run_burst(ctl, plant, params, ones);  // learning busy -> dropped
  (void)run_burst(ctl, plant, params, ones);  // dropped
  EXPECT_EQ(ctl.learning_overruns(), 2U);

  EXPECT_TRUE(ctl.service_learning());
  EXPECT_FALSE(ctl.service_learning());  // nothing pending any more
  EXPECT_EQ(ctl.bursts_learned(), 1U);
}

TEST(RealtimeController, PublishedTableOnlyTakesEffectAtTheNextBurst) {
  const ebc::ControllerParams<double> params{};
  const ebc::ExcimerPlantModel<double> plant{params.plant};
  const double nominal = plant.nominal_command();
  ebc::RealtimeController<double> ctl{params, nominal, Strategy::hybrid_ilc_pid};
  Burst ones{};
  ones.fill(1.0);

  (void)run_burst(ctl, plant, params, ones);
  ctl.begin_burst();  // the burst starts before learning has run
  ASSERT_TRUE(ctl.service_learning());
  // Still the old table within this burst (feedback is zero during the dead time).
  EXPECT_DOUBLE_EQ(ctl.on_pulse(0, params.plant.target_energy_mj).feedforward, nominal);
  ctl.begin_burst();
  EXPECT_NE(ctl.on_pulse(0, params.plant.target_energy_mj).feedforward, nominal);
}

TEST(RealtimeController, OpenLoopAndPidNeverRequestLearning) {
  const ebc::ControllerParams<double> params{};
  const ebc::ExcimerPlantModel<double> plant{params.plant};
  Burst ones{};
  ones.fill(1.0);
  for (const Strategy s : {Strategy::open_loop, Strategy::pid_only}) {
    ebc::RealtimeController<double> ctl{params, plant.nominal_command(), s};
    (void)run_burst(ctl, plant, params, ones);
    EXPECT_FALSE(ctl.service_learning());
    EXPECT_EQ(ctl.learning_overruns(), 0U);
  }
}

}  // namespace
