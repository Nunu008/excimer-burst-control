// SPDX-License-Identifier: MIT
//
// Q-filter of the learning law: a centred moving average with edge padding.
// Mirrors `src/core/filters.py::moving_average` sample for sample.
#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

#include "ebc/core/real.hpp"

namespace ebc {

/// Centred moving average over `depth` samples. The signal is padded with its first
/// and last value (NumPy `mode="edge"`), so the output has the input's length.
///
/// Cost is O(N * depth) with no allocation. `depth` is 3 in practice, so a running
/// sum would save almost nothing and would round differently from the Python model.
///
/// @pre `in` and `out` do not overlap.
template <Real T, std::size_t N>
constexpr void centred_moving_average(std::span<const T, N> in, std::span<T, N> out,
                                      std::size_t depth) noexcept {
  if constexpr (N == 0) {
    return;
  } else {
    if (depth <= 1) {
      std::ranges::copy(in, out.begin());
      return;
    }
    depth = std::min(depth, N);
    const std::size_t pad_left = depth / 2;
    const T scale = T{1} / static_cast<T>(depth);
    for (std::size_t i = 0; i < N; ++i) {
      T acc{0};
      for (std::size_t k = 0; k < depth; ++k) {
        // Index into the virtually padded signal, clamped to the edges.
        const std::size_t virtual_index = i + k;  // offset by pad_left
        const std::size_t j = virtual_index < pad_left ? 0
                              : virtual_index - pad_left >= N ? N - 1
                                                              : virtual_index - pad_left;
        acc += in[j];
      }
      out[i] = acc * scale;
    }
  }
}

}  // namespace ebc
