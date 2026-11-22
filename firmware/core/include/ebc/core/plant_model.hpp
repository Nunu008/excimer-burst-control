// SPDX-License-Identifier: MIT
//
// Software model of the excimer gain medium. On the bench it is replaced by the real
// energy monitor; in CI and in QEMU it closes the loop on the target itself
// (software-in-the-loop on the actual instruction set).
#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include "ebc/core/config.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// eta(i) = exp(-beta * i) * (1 - d * exp(-i / tau))   (README section 3.1)
template <Real T>
[[nodiscard]] inline T gas_efficiency(const PlantParams<T>& p, std::size_t pulse) noexcept {
  const T i = static_cast<T>(pulse);
  const T degradation = std::exp(-p.decay_rate * i);
  const T transient = T{1} - p.startup_transient_depth * std::exp(-i / p.startup_transient_tau);
  return degradation * transient;
}

/// Static, memoryless plant. The efficiency profile is evaluated once at construction,
/// so `emit()` is three multiplications and fits easily into a pulse interrupt.
template <Real T, typename Geometry = DefaultGeometry>
class ExcimerPlantModel {
 public:
  static constexpr std::size_t kPulses = Geometry::pulses;

  explicit ExcimerPlantModel(const PlantParams<T>& params) noexcept : params_{params} {
    for (std::size_t i = 0; i < kPulses; ++i) efficiency_[i] = gas_efficiency(params, i);
  }

  [[nodiscard]] T efficiency(std::size_t pulse) const noexcept { return efficiency_[pulse]; }

  /// Output energy in mJ. `noise_factor` is multiplicative (1 = noise free).
  [[nodiscard]] T emit(T command, std::size_t pulse, T noise_factor = T{1}) const noexcept {
    return max(T{0}, command * params_.actuator_gain * efficiency_[pulse] * noise_factor);
  }

  /// Open-loop command that hits the target on the very first pulse.
  [[nodiscard]] T nominal_command() const noexcept {
    return params_.target_energy_mj / (params_.actuator_gain * efficiency_[0]);
  }

  [[nodiscard]] const PlantParams<T>& params() const noexcept { return params_; }

 private:
  PlantParams<T> params_;
  std::array<T, kPulses> efficiency_{};
};

}  // namespace ebc
