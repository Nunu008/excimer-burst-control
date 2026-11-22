// SPDX-License-Identifier: MIT
//
// Timelog dead time: the energy of pulse `i` is only known at pulse `i + L`.
#pragma once

#include <array>
#include <cstddef>

#include "ebc/core/config.hpp"
#include "ebc/core/real.hpp"

namespace ebc {

/// Fixed-capacity delay line. Until `L` samples have been pushed in the current
/// burst it returns the prefill value (the target energy), exactly like
/// `SimulationEngine._measurement` in the Python model.
template <Real T, std::size_t MaxDelay = kMaxMeasurementDelay>
class DelayLine {
 public:
  constexpr explicit DelayLine(std::size_t delay) noexcept : delay_{delay} {}

  /// Clears the line at a burst boundary and prefills it with `value`.
  constexpr void prefill(T value) noexcept {
    buffer_.fill(value);
    head_ = 0;
  }

  /// Measurement visible at the current pulse (the sample pushed `delay` pulses ago).
  [[nodiscard]] constexpr T read() const noexcept {
    return buffer_[(head_ + kCapacity - delay_) % kCapacity];
  }

  constexpr void push(T sample) noexcept {
    buffer_[head_] = sample;
    head_ = (head_ + 1) % kCapacity;
  }

  [[nodiscard]] constexpr std::size_t delay() const noexcept { return delay_; }

 private:
  static constexpr std::size_t kCapacity = MaxDelay + 1;
  std::array<T, kCapacity> buffer_{};
  std::size_t head_{0};
  std::size_t delay_;
};

}  // namespace ebc
