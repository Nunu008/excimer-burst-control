# 3. C++23 freestanding subset for the control core

* Status: Accepted
* Date: 2026-11-22

## Context

The control law has to run on bare-metal Arm cores (Zynq UltraScale+ APU and RPU,
Cortex-M microcontrollers) and on the development host for testing. Bare-metal
targets have no operating system, a few hundred kilobytes of tightly coupled
memory, and an interrupt with a hard deadline of a fraction of the 500 µs pulse
period at 2 kHz.

## Decision

`firmware/core` is header-only ISO C++23 (also built as C++26 in CI) and uses only
the parts of the language and library that need no run-time support:

* **No heap, no exceptions, no RTTI.** Errors are values (`std::expected`), sizes are
  template parameters, buffers are `std::array`.
* **Templates over the scalar type** (`template <Real T>`): `double` on the host
  for parity with NumPy, `float` on targets with a single-precision FPU.
  `-Wdouble-promotion` and `-Wfloat-conversion` are errors, so a hidden `double`
  operation (a soft-float call in the interrupt) does not compile.
* **`constexpr` by default.** The PID, the learning law, the Q-filter, the KPIs and
  the noise generator are verified by `static_assert` in
  `tests/test_constexpr.cpp`, which also proves they need no run-time support.
* **Modern features where they replace error-prone C idioms:** concepts instead of
  unconstrained templates, designated initialisers for parameter sets, `if consteval`
  for a `sqrt` usable at compile time, `std::span` with static extent instead of
  pointer and length, `std::to_underlying` / scoped enums instead of raw integers.

## Consequences

* Dynamic configuration is limited to what fits a fixed-size burst. That is the
  physics anyway: a burst length change is a new firmware build.
* The same headers compile with the host compiler (GCC 14/15) and the Arm GNU
  toolchain, so unit tests run in milliseconds on the host and the target images
  link the identical code.
