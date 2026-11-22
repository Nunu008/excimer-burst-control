// SPDX-License-Identifier: MIT
//
// Zynq UltraScale+ APU (Cortex-A53) image: the supervisory side of the system. It
// runs the three strategies in lock-step, like the dashboard does, and reports one
// CSV line per burst and strategy on PS UART0. On a ZCU102 that is the first USB-UART
// channel; under QEMU (`-M xlnx-zcu102`) it is stdout.
//
// The verdict (hybrid converged and better than PID) is the semihosting exit code,
// so CI can run the image in QEMU like a unit test.

#include <cstdint>

#include "ebc/app/lockstep_demo.hpp"
#include "ebc/bsp/cadence_uart.hpp"
#include "ebc/bsp/semihosting.hpp"

namespace {

using Uart0 = ebc::bsp::CadenceUart<0xFF00'0000>;

struct UartSink {
  void put(char c) noexcept {
    if (c == '\n') Uart0::put('\r');
    Uart0::put(c);
  }
};

using Engine = ebc::sim::LockstepEngine<float>;
// Static storage: ~5 KiB of burst data never touches the stack.
constinit const ebc::ControllerParams<float> kParams{};
Engine* engine_storage() noexcept {
  static Engine engine{kParams};
  return &engine;
}
Engine::Snapshot snapshot{};

constexpr std::uint32_t kBursts = 20;

}  // namespace

extern "C" [[noreturn]] void ebc_platform_exit(int code) {
  Uart0::flush();
  ebc::bsp::semihosting::exit(code);
}

int main() {
  Uart0::init();
  UartSink sink;
  ebc::app::write(sink, "# excimer-burst-control | Zynq UltraScale+ APU Cortex-A53 | float\n");
  const auto verdict = ebc::app::run_lockstep_demo(sink, *engine_storage(), snapshot,
                                                   kParams.plant.noise_sigma_rel, kBursts, 42);
  ebc::app::write(sink, verdict.passed() ? "# verdict: PASS\n" : "# verdict: FAIL\n");
  return verdict.passed() ? 0 : 1;
}
