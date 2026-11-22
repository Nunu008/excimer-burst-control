// SPDX-License-Identifier: MIT
//
// Cortex-M7 vector table and reset handler, in C23. On ARMv7-M the hardware loads
// the initial stack pointer and the reset vector from the table, so no assembly is
// needed at all.

#include <stdint.h>

extern uint32_t __stack_top[];
[[noreturn]] extern void ebc_c_entry(void);

[[noreturn]] void ebc_reset_handler(void);
void ebc_default_handler(void);
void SysTick_Handler(void);  // defined by the application (main.cpp)

// Every unused exception ends the run with a distinct verdict instead of spinning.
[[noreturn]] extern void ebc_platform_exit(int code);
void ebc_default_handler(void) { ebc_platform_exit(3); }

[[noreturn]] void ebc_reset_handler(void) {
  // Full access to CP10/CP11 (FPU) before any floating-point instruction can run.
  constexpr uintptr_t cpacr = 0xE000ED88U;
  *(volatile uint32_t*)cpacr |= (0xFU << 20);
  __asm__ volatile("dsb\n\tisb" ::: "memory");
  ebc_c_entry();
}

typedef union {
  void (*handler)(void);
  const void* stack_top;
} ebc_vector;

static_assert(sizeof(ebc_vector) == 4);

[[gnu::section(".isr_vector"), gnu::used]] static const ebc_vector ebc_vector_table[16] = {
    {.stack_top = __stack_top},
    {.handler = ebc_reset_handler},
    {.handler = ebc_default_handler},  // NMI
    {.handler = ebc_default_handler},  // HardFault
    {.handler = ebc_default_handler},  // MemManage
    {.handler = ebc_default_handler},  // BusFault
    {.handler = ebc_default_handler},  // UsageFault
    {.handler = nullptr},
    {.handler = nullptr},
    {.handler = nullptr},
    {.handler = nullptr},
    {.handler = ebc_default_handler},  // SVCall
    {.handler = ebc_default_handler},  // DebugMonitor
    {.handler = nullptr},
    {.handler = ebc_default_handler},  // PendSV
    {.handler = SysTick_Handler},      // SysTick: the pulse trigger
};
