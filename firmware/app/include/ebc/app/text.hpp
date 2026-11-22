// SPDX-License-Identifier: MIT
//
// Minimal text output for bare-metal images: no printf, no locale, no heap and no
// floating-point library formatting (std::to_chars for floats would pull ~100 KiB of
// tables into a TCM-sized image). Numbers are printed as fixed point from integers.
#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

#include "ebc/app/hal.hpp"
#include "ebc/core/real.hpp"

namespace ebc::app {

template <hal::CharSink S>
void write(S& sink, std::string_view text) noexcept {
  for (const char c : text) sink.put(c);
}

template <hal::CharSink S, std::integral I>
void write_int(S& sink, I value) noexcept {
  std::array<char, 24> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  write(sink, std::string_view{buffer.data(), result.ptr});
}

/// Writes `value` rounded to `decimals` places, e.g. 21.725 with 3 decimals.
template <hal::CharSink S, Real T>
void write_fixed(S& sink, T value, unsigned decimals) noexcept {
  std::int64_t scale = 1;
  for (unsigned i = 0; i < decimals; ++i) scale *= 10;
  const bool negative = value < T{0};
  const T magnitude = negative ? -value : value;
  const auto scaled =
      static_cast<std::int64_t>(magnitude * static_cast<T>(scale) + real<T>(0.5));
  if (negative && scaled != 0) sink.put('-');
  write_int(sink, scaled / scale);
  if (decimals == 0) return;
  sink.put('.');
  std::int64_t fraction = scaled % scale;
  for (std::int64_t digit = scale / 10; digit > 0; digit /= 10) {
    sink.put(static_cast<char>('0' + fraction / digit));
    fraction %= digit;
  }
}

}  // namespace ebc::app
