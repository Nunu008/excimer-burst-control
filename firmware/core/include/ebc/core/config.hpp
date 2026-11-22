// SPDX-License-Identifier: MIT
//
// Parameters of the pulse-energy controller.
//
// This is the C++ mirror of `src/core/config.py`. The defaults are identical on
// purpose: the parity test (`firmware/tests/test_python_parity.cpp`) replays golden
// vectors exported from the Python model and fails if the two implementations drift.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "ebc/core/real.hpp"

namespace ebc {

/// Burst layout. The feedforward table is shown as a Rows x Cols grid in the UI and
/// stored row-major, so pulse `i` lives at `(i / Cols, i % Cols)`.
template <std::size_t Rows, std::size_t Cols>
struct BurstGeometry {
  static_assert(Rows > 0 && Cols > 0, "a burst needs at least one pulse");
  static constexpr std::size_t rows = Rows;
  static constexpr std::size_t cols = Cols;
  static constexpr std::size_t pulses = Rows * Cols;
};

using DefaultGeometry = BurstGeometry<10, 10>;

/// Upper bound for the Timelog dead time. It sizes the delay line at compile time,
/// so the real-time path never allocates.
inline constexpr std::size_t kMaxMeasurementDelay = 8;

/// Gas-state model of the gain medium (README section 3.1).
template <Real T>
struct PlantParams {
  T target_energy_mj{real<T>(4.0)};
  T repetition_rate_hz{real<T>(2000.0)};
  T decay_rate{real<T>(0.004)};              ///< per-pulse exponential gas degradation
  T startup_transient_depth{real<T>(0.15)};  ///< depth of the burst-start dip
  T startup_transient_tau{real<T>(3.0)};     ///< time constant of the dip, in pulses
  T noise_sigma_rel{real<T>(0.01)};          ///< relative pulse-to-pulse noise (1 sigma)
  T actuator_gain{real<T>(1.0)};             ///< mJ per command unit
};

template <Real T>
struct PidParams {
  T kp{real<T>(0.55)};
  T ki{real<T>(0.06)};
  T kd{real<T>(0.02)};
  T integral_limit{real<T>(40.0)};
  T output_limit{real<T>(6.0)};
};

template <Real T>
struct IlcParams {
  T learning_rate{real<T>(0.6)};           ///< alpha of the update law, (0, 1]
  std::size_t q_filter_depth{3};  ///< centred moving-average window, >= 1
};

template <Real T>
struct ControllerParams {
  PlantParams<T> plant{};
  PidParams<T> pid{};
  IlcParams<T> ilc{};
  std::size_t measurement_delay_pulses{2};  ///< Timelog dead time, 1..kMaxMeasurementDelay
  T tolerance_band{real<T>(0.02)};                   ///< relative band used for settling
  T command_limit{real<T>(12.0)};                    ///< absolute actuator limit of the hybrid
};

enum class ConfigError : std::uint8_t {
  non_positive_target,
  non_positive_repetition_rate,
  non_positive_transient_tau,
  negative_noise,
  non_positive_gain,
  invalid_pid_limits,
  invalid_learning_rate,
  invalid_q_filter_depth,
  invalid_measurement_delay,
  invalid_tolerance_band,
  invalid_command_limit,
};

[[nodiscard]] constexpr std::string_view to_string(ConfigError error) noexcept {
  using enum ConfigError;
  switch (error) {
    case non_positive_target: return "target energy must be positive";
    case non_positive_repetition_rate: return "repetition rate must be positive";
    case non_positive_transient_tau: return "transient time constant must be positive";
    case negative_noise: return "noise sigma must not be negative";
    case non_positive_gain: return "actuator gain must be positive";
    case invalid_pid_limits: return "PID limits must be positive";
    case invalid_learning_rate: return "learning rate must be in (0, 1]";
    case invalid_q_filter_depth: return "Q-filter depth must be >= 1";
    case invalid_measurement_delay: return "measurement delay must be in [1, 8]";
    case invalid_tolerance_band: return "tolerance band must be in (0, 0.5)";
    case invalid_command_limit: return "command limit must be positive";
  }
  return "unknown configuration error";
}

/// Validates a parameter set. It is `constexpr`, so a firmware image with an invalid
/// built-in configuration does not compile (see the `static_assert`s below).
template <Real T>
[[nodiscard]] constexpr std::expected<void, ConfigError> validate(
    const ControllerParams<T>& params) noexcept {
  using enum ConfigError;
  const auto& plant = params.plant;
  if (!(plant.target_energy_mj > T{0})) return std::unexpected{non_positive_target};
  if (!(plant.repetition_rate_hz > T{0})) return std::unexpected{non_positive_repetition_rate};
  if (!(plant.startup_transient_tau > T{0})) return std::unexpected{non_positive_transient_tau};
  if (plant.noise_sigma_rel < T{0}) return std::unexpected{negative_noise};
  if (!(plant.actuator_gain > T{0})) return std::unexpected{non_positive_gain};
  if (!(params.pid.integral_limit > T{0}) || !(params.pid.output_limit > T{0})) {
    return std::unexpected{invalid_pid_limits};
  }
  if (!(params.ilc.learning_rate > T{0}) || params.ilc.learning_rate > T{1}) {
    return std::unexpected{invalid_learning_rate};
  }
  if (params.ilc.q_filter_depth < 1) return std::unexpected{invalid_q_filter_depth};
  if (params.measurement_delay_pulses < 1 ||
      params.measurement_delay_pulses > kMaxMeasurementDelay) {
    return std::unexpected{invalid_measurement_delay};
  }
  if (!(params.tolerance_band > T{0}) || !(params.tolerance_band < real<T>(0.5))) {
    return std::unexpected{invalid_tolerance_band};
  }
  if (!(params.command_limit > T{0})) return std::unexpected{invalid_command_limit};
  return {};
}

/// The three presets of the Python dashboard, available on the target as well.
enum class Preset : std::uint8_t { standard, strong_gas_noise, high_latency };

template <Real T>
[[nodiscard]] constexpr ControllerParams<T> make_preset(Preset preset) noexcept {
  ControllerParams<T> params{};
  switch (preset) {
    case Preset::standard:
      break;
    case Preset::strong_gas_noise:
      params.plant.noise_sigma_rel = real<T>(0.04);
      params.plant.decay_rate = real<T>(0.006);
      params.plant.startup_transient_depth = real<T>(0.22);
      break;
    case Preset::high_latency:
      // Same gains on purpose: only the dead time changes (mirrors config.py).
      params.measurement_delay_pulses = 3;
      break;
  }
  return params;
}

static_assert(validate(ControllerParams<double>{}).has_value());
static_assert(validate(ControllerParams<float>{}).has_value());
static_assert(validate(make_preset<float>(Preset::strong_gas_noise)).has_value());
static_assert(validate(make_preset<float>(Preset::high_latency)).has_value());

}  // namespace ebc
