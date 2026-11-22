// SPDX-License-Identifier: MIT
//
// C23 run-time start shared by every bare-metal target. The architecture-specific
// reset code (startup.S, or the Cortex-M reset handler) sets up the stack and the
// FPU and then calls ebc_c_entry(), which prepares the C/C++ environment.
//
// For images loaded into RAM by a debugger, the FSBL or QEMU, .data is already at
// its run address and the copy is a no-op (load address == run address). For images
// in flash the linker script places the load image elsewhere and the copy is real.

#include <stddef.h>
#include <stdint.h>

typedef void (*ebc_init_fn)(void);

extern uint8_t __data_load[];
extern uint8_t __data_start[];
extern uint8_t __data_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];
extern ebc_init_fn __init_array_start[];
extern ebc_init_fn __init_array_end[];

extern int main(void);
[[noreturn]] extern void ebc_platform_exit(int code);
[[noreturn]] void ebc_c_entry(void);

// Byte loops instead of memcpy/memset: nothing from the C library may run before
// its own .data and .bss exist.
static void copy_bytes(uint8_t* restrict dst, const uint8_t* restrict src, size_t n) {
  for (size_t i = 0; i < n; ++i) dst[i] = src[i];
}

static void zero_bytes(uint8_t* dst, size_t n) {
  for (size_t i = 0; i < n; ++i) dst[i] = 0;
}

[[noreturn]] void ebc_c_entry(void) {
  if ((uintptr_t)__data_load != (uintptr_t)__data_start) {
    copy_bytes(__data_start, __data_load, (size_t)(__data_end - __data_start));
  }
  zero_bytes(__bss_start, (size_t)(__bss_end - __bss_start));

  // Static constructors of the C++ application (none in the control core, which is
  // constexpr-constructible, but a BSP is free to have them).
  for (ebc_init_fn* fn = __init_array_start; fn != __init_array_end; ++fn) (*fn)();

  ebc_platform_exit(main());
}
