// SPDX-License-Identifier: MIT
//
// Compile-time tests. If this file compiles, the assertions hold: the PID law, the
// learning law, the Q-filter, the KPI maths and the noise generator are evaluated by
// the compiler. That is also the proof that none of them needs a heap, exceptions or
// I/O, which is exactly what a bare-metal target requires.

#include <array>

#include <gtest/gtest.h>

#include "ebc/core/config.hpp"
#include "ebc/core/delay_line.hpp"
#include "ebc/core/ilc.hpp"
#include "ebc/core/metrics.hpp"
#include "ebc/core/noise.hpp"
#include "ebc/core/pid.hpp"
#include "ebc/core/q_filter.hpp"

namespace {

constexpr bool near(double a, double b, double tol = 1e-12) { return ebc::abs(a - b) <= tol; }

// PID: proportional term and anti-windup clamp.
static_assert([] {
  ebc::Pid<double> pid{{.kp = 0.5, .ki = 0.0, .kd = 0.0}};
  return near(pid.update(2.0), 1.0);
}());

static_assert([] {
  ebc::Pid<double> pid{{.kp = 0.0, .ki = 1.0, .kd = 0.0, .integral_limit = 5.0}};
  for (int i = 0; i < 100; ++i) (void)pid.update(1.0);
  return near(pid.integral(), 5.0);
}());

// Learning law without smoothing: 4 + 0.5 * 2 = 5 everywhere.
static_assert([] {
  ebc::IterativeLearningController<double, ebc::BurstGeometry<2, 4>> ilc{
      {.learning_rate = 0.5, .q_filter_depth = 1}, 4.0};
  std::array<double, 8> errors{};
  errors.fill(2.0);
  (void)ilc.learn(errors);
  for (const double v : ilc.table()) {
    if (!near(v, 5.0)) return false;
  }
  return ilc.bursts_learned() == 1;
}());

// Q-filter with even depth matches numpy edge padding: [1,2,3,4], q = 4.
static_assert([] {
  const std::array<double, 4> in{1, 2, 3, 4};
  std::array<double, 4> out{};
  ebc::centred_moving_average<double, 4>(in, out, 4);
  return near(out[0], 1.25) && near(out[1], 1.75) && near(out[2], 2.5) && near(out[3], 3.25);
}());

// KPIs, including the constexpr square root.
static_assert([] {
  const std::array<double, 3> energy{3.0, 4.0, 5.0};
  const auto kpis = ebc::compute_kpis<double, 3>(energy, 4.0, 0.02);
  return near(kpis.rms_mj, 0.816496580927726) && near(kpis.max_deviation_pct, 25.0);
}());

// Dead time: the first L reads see the prefill value.
static_assert([] {
  ebc::DelayLine<double> line{2};
  line.prefill(4.0);
  const bool before = near(line.read(), 4.0);
  line.push(1.0);
  const bool still = near(line.read(), 4.0);
  line.push(2.0);
  return before && still && near(line.read(), 1.0);
}());

// The noise generator is deterministic.
static_assert([] {
  ebc::Xoshiro128pp a{42};
  ebc::Xoshiro128pp b{42};
  return a.next() == b.next() && a.next() != a.next();
}());

TEST(Constexpr, AssertionsAboveAreEvaluatedByTheCompiler) { SUCCEED(); }

}  // namespace
