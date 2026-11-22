// SPDX-License-Identifier: MIT
//
// Cadence UART (Zynq UltraScale+ PS UART0/1, also Zynq-7000), polled transmit.
// Register map: Zynq UltraScale+ Device TRM (UG1085), "UART Controller".
#pragma once

#include <cstdint>

#include "ebc/bsp/mmio.hpp"

namespace ebc::bsp {

template <std::uintptr_t Base, std::uint32_t RefClockHz = 100'000'000, std::uint32_t Baud = 115'200>
class CadenceUart {
 public:
  /// 8N1 at `Baud`. Safe to call when the FSBL has already configured the UART.
  static void init() noexcept {
    Control::write(kTxRes | kRxRes | kTxDis | kRxDis);
    Mode::write(kMode8N1);
    BaudGen::write(kBaudGen);
    BaudDiv::write(kBaudDiv);
    Control::write(kTxEn | kRxEn);
  }

  static void put(char c) noexcept {
    while (Status::any(kTxFull)) {
    }
    Fifo::write(static_cast<std::uint8_t>(c));
  }

  /// Blocks until the last byte has left the shift register (before reset/exit).
  static void flush() noexcept {
    while (!Status::any(kTxEmpty)) {
    }
  }

 private:
  using Control = Reg32<Base, 0x00>;
  using Mode = Reg32<Base, 0x04>;
  using BaudGen = Reg32<Base, 0x18>;
  using Status = Reg32<Base, 0x2C, Access::read_only>;
  using Fifo = Reg32<Base, 0x30>;
  using BaudDiv = Reg32<Base, 0x34>;

  static constexpr std::uint32_t kRxRes = 1U << 0;
  static constexpr std::uint32_t kTxRes = 1U << 1;
  static constexpr std::uint32_t kRxEn = 1U << 2;
  static constexpr std::uint32_t kRxDis = 1U << 3;
  static constexpr std::uint32_t kTxEn = 1U << 4;
  static constexpr std::uint32_t kTxDis = 1U << 5;
  static constexpr std::uint32_t kMode8N1 = 0x20;  // CHRL=8 bit, PAR=none, NBSTOP=1
  static constexpr std::uint32_t kTxEmpty = 1U << 3;
  static constexpr std::uint32_t kTxFull = 1U << 4;

  // baud = ref / (CD * (BDIV + 1)); search the divider pair at compile time.
  struct Divisors {
    std::uint32_t cd;
    std::uint32_t bdiv;
  };
  static consteval Divisors divisors() {
    Divisors best{0, 0};
    std::uint32_t best_error = ~0U;
    for (std::uint32_t bdiv = 4; bdiv <= 254; ++bdiv) {
      const std::uint32_t cd = (RefClockHz + Baud * (bdiv + 1) / 2) / (Baud * (bdiv + 1));
      if (cd < 1 || cd > 65535) continue;
      const std::uint32_t actual = RefClockHz / (cd * (bdiv + 1));
      const std::uint32_t error = actual > Baud ? actual - Baud : Baud - actual;
      if (error < best_error) {
        best = {cd, bdiv};
        best_error = error;
      }
    }
    return best;
  }
  static constexpr Divisors kDivisors = divisors();
  static constexpr std::uint32_t kBaudGen = kDivisors.cd;
  static constexpr std::uint32_t kBaudDiv = kDivisors.bdiv;
  static_assert(kBaudGen != 0, "no UART divisor pair reaches the requested baud rate");
  static_assert(RefClockHz / (kBaudGen * (kBaudDiv + 1)) * 100 / Baud >= 98 &&
                    RefClockHz / (kBaudGen * (kBaudDiv + 1)) * 100 / Baud <= 102,
                "baud rate error above 2 %");
};

}  // namespace ebc::bsp
