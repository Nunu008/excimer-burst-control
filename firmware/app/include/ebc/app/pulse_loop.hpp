// SPDX-License-Identifier: MIT
//
// Interrupt-driven software-in-the-loop pulse loop for the real-time cores
// (Cortex-R5F, Cortex-M7).
//
// A hardware timer stands in for the laser trigger at the repetition rate. Every
// tick is one pulse: the interrupt computes the command, the plant model produces
// the energy, the controller records it. After 100 pulses the burst ends and the
// trigger pauses for `kGapPulses` ticks; in that gap the background loop runs the
// learning step and reports the burst. On the bench, `ExcimerPlantModel` and the
// delay line are replaced by the HV DAC and the energy monitor; the controller and
// this timing structure stay identical.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "ebc/core/delay_line.hpp"
#include "ebc/core/metrics.hpp"
#include "ebc/core/noise.hpp"
#include "ebc/core/plant_model.hpp"
#include "ebc/core/realtime_controller.hpp"

namespace ebc::app {

template <Real T>
struct BurstReport {
  std::uint32_t burst{};
  Kpis<T> kpis{};
  T mean_abs_feedback{};
  std::uint32_t learning_overruns{};
  std::uint32_t report_overruns{};
};

template <Real T, typename Geometry = DefaultGeometry>
class SilPulseLoop {
 public:
  static constexpr std::size_t kPulses = Geometry::pulses;
  static constexpr std::size_t kGapPulses = 20;  ///< trigger pause between bursts
  static constexpr std::size_t kTicksPerBurst = kPulses + kGapPulses;

  SilPulseLoop(const ControllerParams<T>& params, std::uint32_t seed) noexcept
      : params_{params},
        plant_{params.plant},
        controller_{params, plant_.nominal_command(), Strategy::hybrid_ilc_pid},
        timelog_{params.measurement_delay_pulses},
        noise_{params.plant.noise_sigma_rel, seed} {}

  // ------------------------------------------------------ interrupt context

  /// Called from the trigger-timer interrupt. Constant time per tick.
  void on_tick() noexcept {
    if (phase_ < kPulses) {
      const std::size_t i = phase_;
      if (i == 0) {
        controller_.begin_burst();
        timelog_.prefill(params_.plant.target_energy_mj);
      }
      const auto a = controller_.on_pulse(i, timelog_.read());
      const T energy = plant_.emit(a.command, i, noise_.next());
      controller_.record(i, energy);
      timelog_.push(energy);
      energy_[write_slot_][i] = energy;
      feedback_[write_slot_][i] = a.feedback;
      if (i + 1 == kPulses) finish_burst();
    }
    phase_ = (phase_ + 1) % kTicksPerBurst;
  }

  // ----------------------------------------------------- background context

  /// Runs pending learning and, if a burst was completed, fills `out`.
  bool service(BurstReport<T>& out) noexcept {
    controller_.service_learning();
    const std::uint8_t slot = ready_slot_.load(std::memory_order_acquire);
    if (slot == kNoSlot) return false;
    out.burst = ready_burst_;
    out.kpis = compute_kpis<T, kPulses>(energy_[slot], params_.plant.target_energy_mj,
                                        params_.tolerance_band);
    T sum{0};
    for (const T v : feedback_[slot]) sum += abs(v);
    out.mean_abs_feedback = sum / static_cast<T>(kPulses);
    out.learning_overruns = controller_.learning_overruns();
    out.report_overruns = report_overruns_.load(std::memory_order_relaxed);
    ready_slot_.store(kNoSlot, std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::uint32_t bursts_completed() const noexcept {
    return bursts_completed_.load(std::memory_order_relaxed);
  }

 private:
  static constexpr std::uint8_t kNoSlot = 0xFF;

  void finish_burst() noexcept {
    controller_.end_burst();
    const std::uint32_t burst = bursts_completed_.load(std::memory_order_relaxed);
    if (ready_slot_.load(std::memory_order_acquire) == kNoSlot) {
      ready_burst_ = burst;
      ready_slot_.store(write_slot_, std::memory_order_release);
      write_slot_ ^= 1U;
    } else {
      report_overruns_.fetch_add(1, std::memory_order_relaxed);
    }
    bursts_completed_.store(burst + 1, std::memory_order_relaxed);
  }

  ControllerParams<T> params_;
  ExcimerPlantModel<T, Geometry> plant_;
  RealtimeController<T, Geometry> controller_;
  DelayLine<T> timelog_;
  PulseNoise<T> noise_;

  std::size_t phase_{0};  // interrupt-owned
  std::array<std::array<T, kPulses>, 2> energy_{};
  std::array<std::array<T, kPulses>, 2> feedback_{};
  std::uint8_t write_slot_{0};  // interrupt-owned
  std::uint32_t ready_burst_{0};
  std::atomic<std::uint8_t> ready_slot_{kNoSlot};
  std::atomic<std::uint32_t> report_overruns_{0};
  std::atomic<std::uint32_t> bursts_completed_{0};
};

}  // namespace ebc::app
