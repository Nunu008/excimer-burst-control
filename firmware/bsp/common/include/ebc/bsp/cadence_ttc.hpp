// SPDX-License-Identifier: MIT
//
// Cadence Triple Timer Counter (Zynq UltraScale+ LPD TTC0-3), counter 1 in interval
// mode: a periodic interrupt that stands in for the laser trigger.
// Register map: Zynq UltraScale+ Register Reference (UG1087), module "ttc".
#pragma once

#include <cstdint>

#include "ebc/bsp/mmio.hpp"

namespace ebc::bsp {

template <std::uintptr_t Base, std::uint32_t RefClockHz = 100'000'000>
class CadenceTtc {
 public:
  /// Periodic interrupt at `rate_hz`, counter 1, no prescaler (32-bit counter).
  static void start_periodic(std::uint32_t rate_hz) noexcept {
    CounterControl::write(kDisable);
    ClockControl::write(0);  // pclk, prescaler off
    Interval::write(RefClockHz / rate_hz - 1);
    (void)InterruptStatus::read();  // clear stale events (clear on read)
    InterruptEnable::write(kIntervalIrq);
    // Interval mode, waveform output off, reset the counter, then run.
    CounterControl::write(kIntervalMode | kWaveDisable | kReset);
  }

  /// Acknowledges the interval interrupt; returns true if it was ours.
  static bool acknowledge() noexcept { return (InterruptStatus::read() & kIntervalIrq) != 0; }

 private:
  using ClockControl = Reg32<Base, 0x00>;
  using CounterControl = Reg32<Base, 0x0C>;
  using Interval = Reg32<Base, 0x24>;
  using InterruptStatus = Reg32<Base, 0x54, Access::read_only>;
  using InterruptEnable = Reg32<Base, 0x60>;

  static constexpr std::uint32_t kDisable = 1U << 0;
  static constexpr std::uint32_t kIntervalMode = 1U << 1;
  static constexpr std::uint32_t kReset = 1U << 4;
  static constexpr std::uint32_t kWaveDisable = 1U << 5;
  static constexpr std::uint32_t kIntervalIrq = 1U << 0;
};

}  // namespace ebc::bsp
