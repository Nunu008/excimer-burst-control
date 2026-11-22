// SPDX-License-Identifier: MIT
//
// Lock-step simulation of the three strategies on one plant and one noise
// realisation per burst. Mirrors `src/core/simulation.py::SimulationEngine` and is
// what the Python parity test and the on-target demo images execute.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "ebc/core/config.hpp"
#include "ebc/core/delay_line.hpp"
#include "ebc/core/hybrid.hpp"
#include "ebc/core/metrics.hpp"
#include "ebc/core/pid.hpp"
#include "ebc/core/plant_model.hpp"
#include "ebc/core/real.hpp"

namespace ebc::sim {

template <Real T, std::size_t N>
struct BurstResult {
  std::array<T, N> energy_mj{};
  std::array<T, N> command{};
  std::array<T, N> feedforward{};
  std::array<T, N> feedback{};
  Kpis<T> kpis{};

  /// Mean |u_fb| of the burst: how much work the feedback still does.
  [[nodiscard]] constexpr T mean_abs_feedback() const noexcept {
    T sum{0};
    for (const T value : feedback) sum += abs(value);
    return sum / static_cast<T>(N);
  }
};

template <Real T, std::size_t N>
struct BurstSnapshot {
  std::uint32_t index{};
  std::array<BurstResult<T, N>, kStrategyCount> results{};

  [[nodiscard]] constexpr const BurstResult<T, N>& result(Strategy s) const noexcept {
    return results[std::to_underlying(s)];
  }
};

template <Real T, typename Geometry = DefaultGeometry>
class LockstepEngine {
 public:
  static constexpr std::size_t kPulses = Geometry::pulses;
  using Snapshot = BurstSnapshot<T, kPulses>;
  using Noise = std::span<const T, kPulses>;

  explicit LockstepEngine(const ControllerParams<T>& params) noexcept
      : params_{params},
        plant_{params.plant},
        pid_only_{params.pid},
        hybrid_{params, plant_.nominal_command()} {}

  /// Simulates one burst with the given multiplicative noise factors and fills `out`.
  /// The snapshot is an out-parameter because it is several kilobytes: on a target
  /// it lives in static memory, never on a small interrupt stack.
  void step(Noise noise, Snapshot& out) noexcept {
    out.index = burst_index_;
    run_open_loop(noise, out.results[0]);
    run_pid_only(noise, out.results[1]);
    run_hybrid(noise, out.results[2]);

    std::array<T, kPulses> errors{};
    const T target = params_.plant.target_energy_mj;
    for (std::size_t i = 0; i < kPulses; ++i) errors[i] = target - out.results[2].energy_mj[i];
    hybrid_.learning().learn(errors);
    ++burst_index_;
  }

  [[nodiscard]] const auto& learning() const noexcept { return hybrid_.learning(); }
  [[nodiscard]] const ExcimerPlantModel<T, Geometry>& plant() const noexcept { return plant_; }
  [[nodiscard]] std::uint32_t bursts_completed() const noexcept { return burst_index_; }

 private:
  using Result = BurstResult<T, kPulses>;

  void run_open_loop(Noise noise, Result& r) noexcept {
    const T command = plant_.nominal_command();
    for (std::size_t i = 0; i < kPulses; ++i) {
      r.energy_mj[i] = plant_.emit(command, i, noise[i]);
      r.command[i] = command;
      r.feedforward[i] = command;
      r.feedback[i] = T{0};
    }
    finish(r);
  }

  void run_pid_only(Noise noise, Result& r) noexcept {
    // No reset: the pure-PID comparison keeps its integral across bursts.
    const T target = params_.plant.target_energy_mj;
    DelayLine<T> timelog{params_.measurement_delay_pulses};
    timelog.prefill(target);
    for (std::size_t i = 0; i < kPulses; ++i) {
      const auto a = pid_only_.act(target, timelog.read(), plant_.nominal_command());
      record(r, i, a, plant_.emit(a.command, i, noise[i]));
      timelog.push(r.energy_mj[i]);
    }
    finish(r);
  }

  void run_hybrid(Noise noise, Result& r) noexcept {
    const T target = params_.plant.target_energy_mj;
    hybrid_.begin_burst();
    DelayLine<T> timelog{params_.measurement_delay_pulses};
    timelog.prefill(target);
    for (std::size_t i = 0; i < kPulses; ++i) {
      const auto a = hybrid_.act(i, target, timelog.read());
      record(r, i, a, plant_.emit(a.command, i, noise[i]));
      timelog.push(r.energy_mj[i]);
    }
    finish(r);
  }

  static void record(Result& r, std::size_t i, const Actuation<T>& a, T energy) noexcept {
    r.command[i] = a.command;
    r.feedforward[i] = a.feedforward;
    r.feedback[i] = a.feedback;
    r.energy_mj[i] = energy;
  }

  void finish(Result& r) const noexcept {
    r.kpis = compute_kpis<T, kPulses>(r.energy_mj, params_.plant.target_energy_mj,
                                      params_.tolerance_band);
  }

  ControllerParams<T> params_;
  ExcimerPlantModel<T, Geometry> plant_;
  Pid<T> pid_only_;
  HybridController<T, Geometry> hybrid_;
  std::uint32_t burst_index_{0};
};

}  // namespace ebc::sim
