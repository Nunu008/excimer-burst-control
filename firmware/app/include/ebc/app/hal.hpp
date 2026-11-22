// SPDX-License-Identifier: MIT
//
// Hardware abstraction as C++20 concepts. A BSP satisfies these with zero-overhead
// static classes; the application is a template over them and never names a
// register. Swapping the Zynq UART for a Cortex-M UART is a type change, checked
// at compile time, with no virtual dispatch in the real-time path.
#pragma once

#include <concepts>
#include <cstdint>

namespace ebc::hal {

/// Byte-oriented output channel (UART, semihosting, a test buffer).
template <typename S>
concept CharSink = requires(S& sink, char c) {
  { sink.put(c) } -> std::same_as<void>;
};

/// How the image reports its verdict to whoever launched it (QEMU via semihosting,
/// a debugger, or nothing on a production board).
template <typename P>
concept Platform = requires(int code) {
  { P::exit(code) } -> std::same_as<void>;
};

}  // namespace ebc::hal
