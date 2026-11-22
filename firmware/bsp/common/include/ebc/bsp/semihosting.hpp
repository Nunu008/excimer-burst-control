// SPDX-License-Identifier: MIT
//
// Arm semihosting exit, used to hand the test verdict of an image to QEMU (or a
// debugger) as a process exit code. Only compiled in with EBC_SEMIHOSTING: on a
// board without a debugger the trap instruction would stop the core.
#pragma once

#include <cstdint>

namespace ebc::bsp::semihosting {

inline constexpr std::uintptr_t kSysExit = 0x18;
inline constexpr std::uintptr_t kApplicationExit = 0x20026;  // ADP_Stopped_ApplicationExit
inline constexpr std::uintptr_t kRunTimeError = 0x20023;     // ADP_Stopped_RunTimeErrorUnknown

#if defined(EBC_SEMIHOSTING)

[[noreturn]] inline void exit(int code) noexcept {
#if defined(__aarch64__)
  // AArch64: SYS_EXIT takes a parameter block {reason, exit code}.
  const std::uint64_t block[2] = {kApplicationExit, static_cast<std::uint64_t>(code)};
  register std::uint64_t x0 asm("x0") = kSysExit;
  register const std::uint64_t* x1 asm("x1") = block;
  asm volatile("hlt #0xf000" : "+r"(x0) : "r"(x1) : "memory");
#else
  // AArch32: the reason alone; QEMU maps ApplicationExit to 0 and anything else to 1.
  register std::uintptr_t r0 asm("r0") = kSysExit;
  register std::uintptr_t r1 asm("r1") = code == 0 ? kApplicationExit : kRunTimeError;
#if defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M'
  asm volatile("bkpt #0xab" : "+r"(r0) : "r"(r1) : "memory");
#elif defined(__thumb__)
  asm volatile("svc #0xab" : "+r"(r0) : "r"(r1) : "memory");
#else
  asm volatile("svc #0x123456" : "+r"(r0) : "r"(r1) : "memory");
#endif
#endif
  for (;;) {
  }
}

#else

[[noreturn]] inline void exit(int /*code*/) noexcept {
  for (;;) {
    asm volatile("wfi");
  }
}

#endif

}  // namespace ebc::bsp::semihosting
