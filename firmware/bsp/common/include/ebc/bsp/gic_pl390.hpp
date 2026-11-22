// SPDX-License-Identifier: MIT
//
// Arm Generic Interrupt Controller v1 (PL390), as used by the Zynq UltraScale+ RPU.
// Only what a single-core bare-metal image needs: route one SPI to CPU 0, enable
// it, acknowledge and end interrupts.
#pragma once

#include <cstdint>

#include "ebc/bsp/mmio.hpp"

namespace ebc::bsp {

template <std::uintptr_t DistBase, std::uintptr_t CpuBase>
class GicPl390 {
 public:
  static constexpr std::uint32_t kSpurious = 1023;

  static void init() noexcept {
    DistCtrl::write(0);
    CpuPriorityMask::write(0xF0);  // accept every priority above the lowest
    CpuCtrl::write(1);
    DistCtrl::write(1);
  }

  static void enable(std::uint32_t id, std::uint8_t priority = 0xA0) noexcept {
    byte_reg(kDistPriority, id) = priority;
    byte_reg(kDistTargets, id) = 0x01;  // CPU interface 0
    word_reg(kDistSetEnable + 4 * (id / 32)) = 1U << (id % 32);
  }

  [[nodiscard]] static std::uint32_t acknowledge() noexcept { return CpuAck::read(); }
  static void end_of_interrupt(std::uint32_t iar) noexcept { CpuEoi::write(iar); }

 private:
  using DistCtrl = Reg32<DistBase, 0x000>;
  using CpuCtrl = Reg32<CpuBase, 0x000>;
  using CpuPriorityMask = Reg32<CpuBase, 0x004>;
  using CpuAck = Reg32<CpuBase, 0x00C, Access::read_only>;
  using CpuEoi = Reg32<CpuBase, 0x010, Access::write_only>;

  static constexpr std::uintptr_t kDistSetEnable = 0x100;
  static constexpr std::uintptr_t kDistPriority = 0x400;
  static constexpr std::uintptr_t kDistTargets = 0x800;

  // Priority and target registers are byte-accessible arrays indexed by interrupt ID.
  static volatile std::uint8_t& byte_reg(std::uintptr_t offset, std::uint32_t id) noexcept {
    return *reinterpret_cast<volatile std::uint8_t*>(DistBase + offset + id);
  }
  static volatile std::uint32_t& word_reg(std::uintptr_t offset) noexcept {
    return *reinterpret_cast<volatile std::uint32_t*>(DistBase + offset);
  }
};

}  // namespace ebc::bsp
