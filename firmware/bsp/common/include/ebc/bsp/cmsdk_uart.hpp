// SPDX-License-Identifier: MIT
//
// Arm CMSDK APB UART (MPS2 boards and many Cortex-M FPGA subsystems), polled TX.
#pragma once

#include <cstdint>

#include "ebc/bsp/mmio.hpp"

namespace ebc::bsp {

template <std::uintptr_t Base, std::uint32_t ClockHz = 25'000'000, std::uint32_t Baud = 115'200>
class CmsdkUart {
 public:
  static void init() noexcept {
    BaudDiv::write(ClockHz / Baud);
    Control::write(kTxEnable);
  }

  static void put(char c) noexcept {
    while (State::any(kTxFull)) {
    }
    Data::write(static_cast<std::uint8_t>(c));
  }

  static void flush() noexcept {
    while (State::any(kTxFull)) {
    }
  }

 private:
  using Data = Reg32<Base, 0x000>;
  using State = Reg32<Base, 0x004, Access::read_only>;
  using Control = Reg32<Base, 0x008>;
  using BaudDiv = Reg32<Base, 0x010>;

  static constexpr std::uint32_t kTxFull = 1U << 0;
  static constexpr std::uint32_t kTxEnable = 1U << 0;
  static_assert(ClockHz / Baud >= 16, "CMSDK UART needs BAUDDIV >= 16");
};

}  // namespace ebc::bsp
