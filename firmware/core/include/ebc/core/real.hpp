// SPDX-License-Identifier: MIT
//
// Numeric foundation shared by every control component.
#pragma once

#include <cmath>
#include <concepts>

namespace ebc {

/// Scalar type of all control computations. The host build instantiates `double` to
/// match NumPy within rounding; targets with a single-precision FPU (Cortex-R5F,
/// Cortex-M7) instantiate `float`.
template <typename T>
concept Real = std::floating_point<T>;

/// Converts a decimal constant to `T` at compile time. Parameter defaults such as
/// 0.004 are not representable in `float`; this makes the rounding to the nearest
/// `float` an explicit, reviewed decision instead of a silent narrowing that
/// -Wfloat-conversion would (rightly) reject.
template <Real T>
[[nodiscard]] consteval T real(long double value) noexcept {
  return static_cast<T>(value);
}

template <Real T>
[[nodiscard]] constexpr T clamp(T value, T lo, T hi) noexcept {
  return value < lo ? lo : (hi < value ? hi : value);
}

template <Real T>
[[nodiscard]] constexpr T abs(T value) noexcept {
  return value < T{0} ? -value : value;
}

template <Real T>
[[nodiscard]] constexpr T max(T a, T b) noexcept {
  return a < b ? b : a;
}

/// Square root that also works in constant expressions, so the KPI maths can be
/// unit tested with `static_assert`. At run time it is the libm / FPU instruction.
template <Real T>
[[nodiscard]] constexpr T sqrt(T value) noexcept {
  if consteval {
    if (!(value > T{0})) return T{0};
    T x = value < T{1} ? T{1} : value;
    for (int i = 0; i < 128; ++i) {
      const T next = real<T>(0.5) * (x + value / x);
      if (next == x) break;
      x = next;
    }
    return x;
  } else {
    return std::sqrt(value);
  }
}

}  // namespace ebc
