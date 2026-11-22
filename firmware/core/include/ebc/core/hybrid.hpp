// SPDX-License-Identifier: MIT
//
// Hybrid ILC + PID controller. Mirrors `src/core/controllers.py::HybridController`.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ebc/core/config.hpp"
#include "ebc/core/ilc.hpp"
#include "ebc/core/pid.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

enum class Strategy : std::uint8_t { open_loop = 0, pid_only = 1, hybrid_ilc_pid = 2 };

inline constexpr std::size_t kStrategyCount = 3;

[[nodiscard]] constexpr std::string_view to_string(Strategy strategy) noexcept {
  switch (strategy) {
    case Strategy::open_loop: return "hv_open_loop";
    case Strategy::pid_only: return "pid_only";
    case Strategy::hybrid_ilc_pid: return "hybrid_ilr_pid";  // key used by the Python model
  }
  return "unknown";
}

/// Combines a learned feedforward value with the PID trim and applies the actuator
/// limits. Shared by the simulation and the interrupt-driven controller.
template <Real T>
[[nodiscard]] constexpr Actuation<T> hybrid_actuation(T feedforward, T feedback,
                                                      T command_limit) noexcept {
  return {.command = clamp(feedforward + feedback, T{0}, command_limit),
          .feedforward = feedforward,
          .feedback = feedback};
}

template <Real T, typename Geometry = DefaultGeometry>
class HybridController {
 public:
  using Learning = IterativeLearningController<T, Geometry>;

  constexpr HybridController(const ControllerParams<T>& params, T initial_command) noexcept
      : learning_{params.ilc, initial_command},
        pid_{params.pid},
        command_limit_{params.command_limit} {}

  /// Clears the PID state at a burst boundary. The repetitive part of the
  /// disturbance belongs to the learned table; a cross-burst integral would carry a
  /// constant offset that the table can never absorb (README section 8).
  constexpr void begin_burst() noexcept { pid_.reset(); }

  [[nodiscard]] constexpr Actuation<T> act(std::size_t pulse, T target, T measurement) noexcept {
    return hybrid_actuation(learning_.command(pulse), pid_.update(target - measurement),
                            command_limit_);
  }

  constexpr void reset() noexcept {
    learning_.reset();
    pid_.reset();
  }

  [[nodiscard]] constexpr Learning& learning() noexcept { return learning_; }
  [[nodiscard]] constexpr const Learning& learning() const noexcept { return learning_; }

 private:
  Learning learning_;
  Pid<T> pid_;
  T command_limit_;
};

}  // namespace ebc
