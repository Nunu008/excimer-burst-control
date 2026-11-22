// SPDX-License-Identifier: MIT
//
// Cortex-M7 image: the same interrupt-driven pulse loop as the Zynq RPU, on a
// microcontroller. SysTick at 2 kHz is the pulse trigger; the background loop runs
// the learning step and reports each burst on UART0.

#include <cstdint>

#include "ebc/app/pulse_loop.hpp"
#include "ebc/app/report.hpp"
#include "ebc/bsp/cmsdk_uart.hpp"
#include "ebc/bsp/mmio.hpp"
#include "ebc/bsp/semihosting.hpp"

namespace {

constexpr std::uint32_t kCoreClockHz = 25'000'000;  // MPS2 AN500 SYSCLK
constexpr std::uint32_t kRepetitionRateHz = 2000;
constexpr std::uint32_t kBursts = 20;

using Uart0 = ebc::bsp::CmsdkUart<0x4000'4000, kCoreClockHz>;

// ARMv7-M SysTick (architecturally fixed addresses).
using SysTickCtrl = ebc::bsp::Reg32<0xE000'E010, 0x0>;
using SysTickLoad = ebc::bsp::Reg32<0xE000'E014, 0x0>;
using SysTickValue = ebc::bsp::Reg32<0xE000'E018, 0x0>;
constexpr std::uint32_t kSysTickEnable = 1U << 0;
constexpr std::uint32_t kSysTickInterrupt = 1U << 1;
constexpr std::uint32_t kSysTickCoreClock = 1U << 2;
static_assert(kCoreClockHz / kRepetitionRateHz - 1 <= 0x00FF'FFFF, "SysTick reload is 24 bit");

struct UartSink {
  void put(char c) noexcept {
    if (c == '\n') Uart0::put('\r');
    Uart0::put(c);
  }
};

using Loop = ebc::app::SilPulseLoop<float>;
Loop& pulse_loop() noexcept {
  static Loop loop{ebc::ControllerParams<float>{}, 42U};
  return loop;
}

}  // namespace

extern "C" [[noreturn]] void ebc_platform_exit(int code) {
  Uart0::flush();
  ebc::bsp::semihosting::exit(code);
}

extern "C" void SysTick_Handler() { pulse_loop().on_tick(); }

int main() {
  Uart0::init();
  UartSink sink;
  ebc::app::write(sink, "# excimer-burst-control | Cortex-M7 | SysTick IRQ 2 kHz\n");
  ebc::app::write_header(sink);

  Loop& loop = pulse_loop();  // construct before the first interrupt can fire
  SysTickLoad::write(kCoreClockHz / kRepetitionRateHz - 1);
  SysTickValue::write(0);
  SysTickCtrl::write(kSysTickEnable | kSysTickInterrupt | kSysTickCoreClock);

  ebc::app::BurstReport<float> report{};
  for (;;) {
    if (loop.service(report)) {
      ebc::app::write_burst_line(sink, report.burst, ebc::Strategy::hybrid_ilc_pid, report.kpis,
                                 report.mean_abs_feedback);
      if (report.burst + 1 >= kBursts) break;
    } else {
      asm volatile("wfi");
    }
  }
  SysTickCtrl::write(0);

  const bool passed = report.kpis.max_deviation_pct < 5.0F && report.learning_overruns == 0;
  ebc::app::write(sink, "# learning overruns: ");
  ebc::app::write_int(sink, report.learning_overruns);
  ebc::app::write(sink, passed ? "\n# verdict: PASS\n" : "\n# verdict: FAIL\n");
  return passed ? 0 : 1;
}
