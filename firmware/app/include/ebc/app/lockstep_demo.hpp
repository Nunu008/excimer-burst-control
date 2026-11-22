// SPDX-License-Identifier: MIT
//
// Supervisory demo: the three strategies in lock-step, as on the dashboard. Runs on
// the Zynq UltraScale+ APU (Cortex-A53), where comparison and analytics live in a
// real system, while the RPU runs the hard real-time loop. The host simulator runs
// the identical function, so their logs are directly comparable.
#pragma once

#include <array>
#include <cstdint>

#include "ebc/app/report.hpp"
#include "ebc/core/noise.hpp"
#include "ebc/sim/lockstep_engine.hpp"

namespace ebc::app {

struct DemoVerdict {
  bool converged;  ///< hybrid inside 5 % max deviation in the last burst
  bool beats_pid;  ///< hybrid RMS below half of the pure PID in the last burst

  [[nodiscard]] constexpr bool passed() const noexcept { return converged && beats_pid; }
};

/// Runs `bursts` bursts, prints one line per burst and strategy, returns a verdict.
/// Engine and snapshot are passed in because they are several kilobytes: a target
/// keeps them in static memory, never on a small stack.
template <Real T, hal::CharSink S, typename Geometry>
DemoVerdict run_lockstep_demo(S& sink, sim::LockstepEngine<T, Geometry>& engine,
                              typename sim::LockstepEngine<T, Geometry>::Snapshot& snapshot,
                              T noise_sigma, std::uint32_t bursts, std::uint32_t seed) noexcept {
  PulseNoise<T> noise{noise_sigma, seed};
  std::array<T, Geometry::pulses> factors{};

  write_header(sink);
  for (std::uint32_t b = 0; b < bursts; ++b) {
    for (T& f : factors) f = noise.next();
    engine.step(factors, snapshot);
    for (std::size_t s = 0; s < kStrategyCount; ++s) {
      const auto& r = snapshot.results[s];
      write_burst_line(sink, snapshot.index, static_cast<Strategy>(s), r.kpis,
                       r.mean_abs_feedback());
    }
  }
  const auto& hybrid = snapshot.result(Strategy::hybrid_ilc_pid).kpis;
  const auto& pid = snapshot.result(Strategy::pid_only).kpis;
  return {.converged = hybrid.max_deviation_pct < real<T>(5.0),
          .beats_pid = hybrid.rms_mj < real<T>(0.5) * pid.rms_mj};
}

}  // namespace ebc::app
