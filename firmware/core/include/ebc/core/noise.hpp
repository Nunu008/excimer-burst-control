// SPDX-License-Identifier: MIT
//
// Deterministic, allocation-free pulse-noise source for on-target simulation.
#pragma once

#include <array>
#include <bit>
#include <cstdint>

#include "ebc/core/real.hpp"

namespace ebc {

/// xoshiro128++ (Blackman & Vigna). 32-bit state words, so it is cheap on Cortex-R/M
/// cores, and it is fully `constexpr`, so sequences can be pinned in unit tests.
class Xoshiro128pp {
 public:
  constexpr explicit Xoshiro128pp(std::uint32_t seed) noexcept {
    // SplitMix32-style expansion of the seed into four non-zero state words.
    std::uint32_t z = seed;
    for (auto& word : state_) {
      z += 0x9E3779B9u;
      std::uint32_t x = z;
      x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
      x = (x ^ (x >> 13)) * 0xC2B2AE35u;
      word = x ^ (x >> 16);
    }
  }

  [[nodiscard]] constexpr std::uint32_t next() noexcept {
    const std::uint32_t result = std::rotl(state_[0] + state_[3], 7) + state_[0];
    const std::uint32_t t = state_[1] << 9;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = std::rotl(state_[3], 11);
    return result;
  }

  /// Uniform sample in [0, 1) with 24 bits of resolution (exact in `float`).
  template <Real T>
  [[nodiscard]] constexpr T uniform() noexcept {
    return static_cast<T>(next() >> 8) * (T{1} / T{16777216});
  }

 private:
  std::array<std::uint32_t, 4> state_{};
};

/// Multiplicative pulse noise `N(1, sigma)`, clipped at 0.2 like the Python plant.
///
/// The normal variate uses the Irwin-Hall approximation (sum of 12 uniforms minus 6):
/// constant time, no libm, branch-free. Its tails end at +/-6 sigma, which is
/// irrelevant for a 1-4 % pulse noise and welcome in an interrupt handler.
template <Real T>
class PulseNoise {
 public:
  constexpr PulseNoise(T sigma_rel, std::uint32_t seed) noexcept : sigma_{sigma_rel}, rng_{seed} {}

  [[nodiscard]] constexpr T next() noexcept {
    if (!(sigma_ > T{0})) return T{1};
    T sum{0};
    for (int i = 0; i < 12; ++i) sum += rng_.uniform<T>();
    return max(real<T>(0.2), T{1} + sigma_ * (sum - T{6}));
  }

 private:
  T sigma_;
  Xoshiro128pp rng_;
};

}  // namespace ebc
