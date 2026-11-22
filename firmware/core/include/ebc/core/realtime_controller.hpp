// SPDX-License-Identifier: MIT
//
// Interrupt-safe pulse-energy controller for the target.
//
// The Python model runs the learning step synchronously between bursts. On a real
// laser the per-pulse path runs in an interrupt at 2-6 kHz and must have a bounded,
// short execution time, while the learning step (O(N * q)) must not stretch it.
// This class splits the two:
//
//   pulse interrupt (wait-free, O(1))        background loop
//   ---------------------------------        -------------------------------
//   begin_burst()  acquire latest table
//   on_pulse(i)    u = u_ff[i] + PID
//   record(i, E)   store error
//   end_burst()    hand errors over  ───────▶ service_learning()
//                                              u_ff' = u_ff + a*Q(e) into shadow
//                  next begin_burst() ◀────── publish shadow (release)
//
// Two invariants make this race free without locks or masking interrupts:
//  1. The learning task only writes the table that is *not* published.
//  2. There is at most one pending hand-over, so at most one publish can happen
//     while a burst is running. The interrupt therefore never reads a table that is
//     being written. A burst that ends while learning is still busy is dropped from
//     learning and counted in `learning_overruns()`; it is never blocked on.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ebc/core/config.hpp"
#include "ebc/core/hybrid.hpp"
#include "ebc/core/ilc.hpp"
#include "ebc/core/pid.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// Double-buffered feedforward table with a single writer (learning task) and a
/// single reader (pulse interrupt).
template <Real T, std::size_t N>
class TableBank {
 public:
  using Table = std::array<T, N>;

  constexpr explicit TableBank(T initial_command) noexcept {
    for (auto& table : tables_) table.fill(initial_command);
  }

  /// Interrupt side: the table to use for the burst that is about to start.
  [[nodiscard]] const Table& acquire() const noexcept {
    return tables_[published_.load(std::memory_order_acquire)];
  }

  /// Learning side: the most recently published table (read only).
  [[nodiscard]] const Table& published() const noexcept {
    return tables_[published_.load(std::memory_order_relaxed)];
  }

  /// Learning side: the table that may be written.
  [[nodiscard]] Table& shadow() noexcept {
    return tables_[published_.load(std::memory_order_relaxed) ^ 1u];
  }

  /// Learning side: makes the shadow table visible to the next `acquire()`.
  void publish() noexcept {
    const auto next = static_cast<std::uint8_t>(published_.load(std::memory_order_relaxed) ^ 1u);
    published_.store(next, std::memory_order_release);
  }

 private:
  std::array<Table, 2> tables_{};
  std::atomic<std::uint8_t> published_{0};
};

template <Real T, typename Geometry = DefaultGeometry>
class RealtimeController {
 public:
  static constexpr std::size_t kPulses = Geometry::pulses;
  using Table = std::array<T, kPulses>;

  RealtimeController(const ControllerParams<T>& params, T nominal_command,
                     Strategy strategy) noexcept
      : params_{params},
        nominal_command_{nominal_command},
        strategy_{strategy},
        pid_{params.pid},
        bank_{nominal_command} {}

  // ---------------------------------------------------- pulse-interrupt context

  void begin_burst() noexcept {
    active_ = &bank_.acquire();
    // The pure-PID comparison keeps its integral across bursts (as in the Python
    // model); the hybrid resets it, see HybridController::begin_burst().
    if (strategy_ == Strategy::hybrid_ilc_pid) pid_.reset();
  }

  /// Command for `pulse` given the measurement that is visible now (already delayed
  /// by the Timelog dead time).
  [[nodiscard]] Actuation<T> on_pulse(std::size_t pulse, T delayed_measurement) noexcept {
    const T target = params_.plant.target_energy_mj;
    switch (strategy_) {
      case Strategy::open_loop:
        return {.command = nominal_command_, .feedforward = nominal_command_, .feedback = T{0}};
      case Strategy::pid_only:
        return pid_.act(target, delayed_measurement, nominal_command_);
      case Strategy::hybrid_ilc_pid:
        return hybrid_actuation((*active_)[pulse], pid_.update(target - delayed_measurement),
                                params_.command_limit);
    }
    return {};
  }

  /// Records the true energy of `pulse` for the learning step.
  void record(std::size_t pulse, T energy) noexcept {
    errors_[write_slot_][pulse] = params_.plant.target_energy_mj - energy;
  }

  void end_burst() noexcept {
    if (strategy_ != Strategy::hybrid_ilc_pid) return;
    if (pending_slot_.load(std::memory_order_acquire) != kNoSlot) {
      // Learning is still busy with the previous burst: drop this one rather than
      // wait. The pending slot is the other one, so this slot stays ours.
      learning_overruns_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    pending_slot_.store(write_slot_, std::memory_order_release);
    write_slot_ ^= 1u;
  }

  // -------------------------------------------------------- background context

  /// Runs one learning step if a burst is pending. Returns true if a new table was
  /// published.
  bool service_learning() noexcept {
    const std::uint8_t slot = pending_slot_.load(std::memory_order_acquire);
    if (slot == kNoSlot) return false;
    last_correction_ = ilc_update<T, kPulses>(params_.ilc, bank_.published(), errors_[slot],
                                              bank_.shadow());
    bank_.publish();
    bursts_learned_.fetch_add(1, std::memory_order_relaxed);
    pending_slot_.store(kNoSlot, std::memory_order_release);
    return true;
  }

  [[nodiscard]] const Table& published_table() const noexcept { return bank_.published(); }
  [[nodiscard]] std::uint32_t bursts_learned() const noexcept {
    return bursts_learned_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint32_t learning_overruns() const noexcept {
    return learning_overruns_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] T last_correction() const noexcept { return last_correction_; }
  [[nodiscard]] Strategy strategy() const noexcept { return strategy_; }

 private:
  static constexpr std::uint8_t kNoSlot = 0xFF;

  ControllerParams<T> params_;
  T nominal_command_;
  Strategy strategy_;
  Pid<T> pid_;
  TableBank<T, kPulses> bank_;
  const Table* active_{&bank_.acquire()};

  std::array<Table, 2> errors_{};
  std::uint8_t write_slot_{0};                        // interrupt-owned
  std::atomic<std::uint8_t> pending_slot_{kNoSlot};   // hand-over interrupt -> background
  std::atomic<std::uint32_t> bursts_learned_{0};
  std::atomic<std::uint32_t> learning_overruns_{0};
  T last_correction_{0};
};

}  // namespace ebc
