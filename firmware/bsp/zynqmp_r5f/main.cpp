// SPDX-License-Identifier: MIT
//
// Zynq UltraScale+ RPU (Cortex-R5F) image: the hard real-time pulse loop.
//
//   TTC0 counter 1 @ 2 kHz ──IRQ 68──▶ GIC ──▶ ebc_irq_dispatch()
//                                              └▶ SilPulseLoop::on_tick()   (O(1))
//   main loop:  service(): learning step + KPI report over PS UART0
//
// On the bench the plant model inside SilPulseLoop is replaced by the HV DAC and
// the energy monitor in the PL; the interrupt structure stays the same.

#include <cstdint>

#include "ebc/app/pulse_loop.hpp"
#include "ebc/app/report.hpp"
#include "ebc/bsp/cadence_ttc.hpp"
#include "ebc/bsp/cadence_uart.hpp"
#include "ebc/bsp/gic_pl390.hpp"
#include "ebc/bsp/semihosting.hpp"

namespace {

using Uart0 = ebc::bsp::CadenceUart<0xFF00'0000>;
using Ttc0 = ebc::bsp::CadenceTtc<0xFF11'0000>;
using Gic = ebc::bsp::GicPl390<0xF900'0000, 0xF900'1000>;
constexpr std::uint32_t kTtc0Counter1Irq = 68;
constexpr std::uint32_t kRepetitionRateHz = 2000;
constexpr std::uint32_t kBursts = 20;

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

extern "C" void ebc_irq_dispatch() {
  const std::uint32_t iar = Gic::acknowledge();
  if ((iar & 0x3FFU) == kTtc0Counter1Irq && Ttc0::acknowledge()) pulse_loop().on_tick();
  if ((iar & 0x3FFU) != Gic::kSpurious) Gic::end_of_interrupt(iar);
}

extern "C" [[noreturn]] void ebc_fault() { ebc_platform_exit(3); }

int main() {
  Uart0::init();
  UartSink sink;
  ebc::app::write(sink, "# excimer-burst-control | Zynq UltraScale+ RPU Cortex-R5F | TTC IRQ 2 kHz\n");
  ebc::app::write_header(sink);

  Loop& loop = pulse_loop();  // construct before the first interrupt can fire
  Gic::init();
  Gic::enable(kTtc0Counter1Irq);
  Ttc0::start_periodic(kRepetitionRateHz);
  asm volatile("cpsie i" ::: "memory");

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
  asm volatile("cpsid i" ::: "memory");

  const bool passed = report.kpis.max_deviation_pct < 5.0F && report.learning_overruns == 0;
  ebc::app::write(sink, "# learning overruns: ");
  ebc::app::write_int(sink, report.learning_overruns);
  ebc::app::write(sink, passed ? "\n# verdict: PASS\n" : "\n# verdict: FAIL\n");
  return passed ? 0 : 1;
}
