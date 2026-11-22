// SPDX-License-Identifier: MIT
//
// Control-quality KPIs of one burst. Mirrors `src/core/metrics.py`.
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "ebc/core/q_filter.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// Settling is judged on a 5-pulse moving average so that single noisy pulses do not
/// dominate the result.
inline constexpr std::size_t kSettlingSmoothingPulses = 5;

template <Real T>
struct Kpis {
  T max_deviation_pct{};
  T std_mj{};
  T rms_mj{};
  std::optional<std::size_t> settling_pulse{};

  [[nodiscard]] constexpr bool settled() const noexcept { return settling_pulse.has_value(); }
};

/// First pulse after which the smoothed absolute error stays inside `band_abs`.
/// Returns 0 if the burst never leaves the band and `nullopt` if it never settles.
template <Real T, std::size_t N>
[[nodiscard]] constexpr std::optional<std::size_t> settling_pulse(std::span<const T, N> errors,
                                                                  T band_abs) noexcept {
  std::array<T, N> smoothed{};
  centred_moving_average<T, N>(errors, smoothed, kSettlingSmoothingPulses);
  std::optional<std::size_t> last_violation{};
  for (std::size_t i = 0; i < N; ++i) {
    if (abs(smoothed[i]) > band_abs) last_violation = i;
  }
  if (!last_violation) return std::size_t{0};
  if (*last_violation + 1 >= N) return std::nullopt;
  return *last_violation + 1;
}

/// Deviation, noise and settling KPIs for one burst of measured energies.
/// @pre `target > 0`.
template <Real T, std::size_t N>
[[nodiscard]] constexpr Kpis<T> compute_kpis(std::span<const T, N> energy, T target,
                                             T tolerance_band) noexcept {
  static_assert(N > 0, "KPIs need at least one pulse");
  std::array<T, N> errors{};
  T largest{0};
  T sum{0};
  T sum_sq{0};
  for (std::size_t i = 0; i < N; ++i) {
    errors[i] = energy[i] - target;
    largest = max(largest, abs(errors[i]));
    sum += errors[i];
    sum_sq += errors[i] * errors[i];
  }
  const T n = static_cast<T>(N);
  const T mean = sum / n;
  T variance{0};
  for (const T e : errors) variance += (e - mean) * (e - mean);
  variance /= n;  // population variance, like numpy.std

  return {.max_deviation_pct = largest / target * T{100},
          .std_mj = sqrt(variance),
          .rms_mj = sqrt(sum_sq / n),
          .settling_pulse = settling_pulse<T, N>(errors, tolerance_band * target)};
}

}  // namespace ebc
