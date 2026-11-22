// SPDX-License-Identifier: MIT
//
// Iterative Learning Control on a per-pulse feedforward table. Mirrors
// `src/core/controllers.py::IterativeLearningController`.
//
//     u_ff,k+1[i] = max(0, u_ff,k[i] + alpha * Q(e_k)[i])
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ebc/core/config.hpp"
#include "ebc/core/q_filter.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// One learning step as a pure function, shared by the single-buffered controller
/// used in simulation and the double-buffered `TableBank` used under interrupts.
///
/// `current` and `next` may refer to the same table: every element is read before
/// it is written. Returns the largest absolute correction before clamping, the
/// same quantity the Python implementation reports.
template <Real T, std::size_t N>
constexpr T ilc_update(const IlcParams<T>& params, std::span<const T, N> current,
                       std::span<const T, N> errors, std::span<T, N> next) noexcept {
  std::array<T, N> filtered{};
  centred_moving_average<T, N>(errors, filtered, params.q_filter_depth);
  T largest{0};
  for (std::size_t i = 0; i < N; ++i) {
    const T delta = params.learning_rate * filtered[i];
    next[i] = max(T{0}, current[i] + delta);
    largest = max(largest, abs(delta));
  }
  return largest;
}

template <Real T, typename Geometry = DefaultGeometry>
class IterativeLearningController {
 public:
  static constexpr std::size_t kPulses = Geometry::pulses;
  using Table = std::array<T, kPulses>;

  constexpr IterativeLearningController(const IlcParams<T>& params, T initial_command) noexcept
      : params_{params}, initial_command_{initial_command} {
    table_.fill(initial_command);
  }

  /// Feedforward command for `pulse`. @pre `pulse < kPulses`.
  [[nodiscard]] constexpr T command(std::size_t pulse) const noexcept { return table_[pulse]; }

  /// Applies one learning step from the errors (`target - measured`) of a burst.
  constexpr T learn(std::span<const T, kPulses> errors) noexcept {
    ++bursts_learned_;
    return ilc_update<T, kPulses>(params_, table_, errors, table_);
  }

  constexpr void reset() noexcept {
    table_.fill(initial_command_);
    bursts_learned_ = 0;
  }

  [[nodiscard]] constexpr const Table& table() const noexcept { return table_; }
  [[nodiscard]] constexpr std::uint32_t bursts_learned() const noexcept { return bursts_learned_; }

  /// Grid view used by telemetry: element at `(row, col)` of the Rows x Cols matrix.
  [[nodiscard]] constexpr T at(std::size_t row, std::size_t col) const noexcept {
    return table_[row * Geometry::cols + col];
  }

 private:
  IlcParams<T> params_;
  T initial_command_;
  Table table_{};
  std::uint32_t bursts_learned_{0};
};

}  // namespace ebc
