// SPDX-License-Identifier: MIT
//
// Discrete PID on the pulse-to-pulse energy error. Mirrors
// `src/core/controllers.py::PIDController`.
#pragma once

#include "ebc/core/config.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// One pulse of control action, split into its physical contributions.
template <Real T>
struct Actuation {
  T command{};
  T feedforward{};
  T feedback{};
};

/// PID with integral clamping (anti-windup) and output clamping.
///
/// Worst-case execution time is constant: no branches depend on history length and
/// there is no division. It is meant to run inside the pulse interrupt.
template <Real T>
class Pid {
 public:
  constexpr explicit Pid(const PidParams<T>& params) noexcept : params_{params} {}

  [[nodiscard]] constexpr T update(T error) noexcept {
    integral_ = clamp(integral_ + error, -params_.integral_limit, params_.integral_limit);
    const T derivative = error - previous_error_;
    previous_error_ = error;
    const T output = params_.kp * error + params_.ki * integral_ + params_.kd * derivative;
    return clamp(output, -params_.output_limit, params_.output_limit);
  }

  /// Full actuation around a fixed baseline command (the "PID only" strategy).
  [[nodiscard]] constexpr Actuation<T> act(T target, T measurement, T base_command) noexcept {
    const T feedback = update(target - measurement);
    return {.command = max(T{0}, base_command + feedback),
            .feedforward = base_command,
            .feedback = feedback};
  }

  constexpr void reset() noexcept {
    integral_ = T{0};
    previous_error_ = T{0};
  }

  [[nodiscard]] constexpr T integral() const noexcept { return integral_; }
  [[nodiscard]] constexpr const PidParams<T>& params() const noexcept { return params_; }

 private:
  PidParams<T> params_;
  T integral_{0};
  T previous_error_{0};
};

}  // namespace ebc
