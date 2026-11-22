# Firmware: pulse-energy controller for bare-metal Arm targets

The control law from `src/core` (Python) in C++23 and C23, for the Zynq UltraScale+
APU and RPU, Cortex-M7 microcontrollers and the development host. The design rules
are in [ADR 0003](../docs/adr/0003-cpp23-freestanding-subset.md); the obligation to
match Python is [ADR 0002](../docs/adr/0002-python-model-as-executable-specification.md).

## Layout

| Path | Content | Depends on |
|---|---|---|
| `core/include/ebc/core/` | PID, ILC + Q-filter, delay line, plant model, KPIs, noise, `RealtimeController` | C++23 standard headers only |
| `core/include/ebc/sim/` | `LockstepEngine`: the three strategies on one noise realisation (mirror of `simulation.py`) | core |
| `app/include/ebc/app/` | HAL concepts, printf-free text output, burst report, lock-step demo, ISR pulse loop | core |
| `capi/` | `ebc.h`, the C23 interface, and its implementation | core |
| `bsp/common/` | `crt.c` and `syscalls.c` (C23), `mmio.hpp`, Cadence UART/TTC, GIC PL390, CMSDK UART, semihosting | — |
| `bsp/zynqmp_a53/` | AArch64 startup (any EL), linker script, supervisory image | app, common |
| `bsp/zynqmp_r5f/` | ARMv7-R vectors and IRQ entry, TCM linker script, interrupt-driven image | app, common |
| `bsp/cortex_m7/` | C23 vector table and reset handler, SysTick-driven image | app, common |
| `bsp/host/` | `ebc_host_sim`, the same application on the PC | app |
| `tests/` | GoogleTest suite, Python parity, C API test, golden vectors | core, app, capi |
| `tools/` | `run_qemu.py` (image as test), `compare_logs.py` (A53 vs host) | Python 3 |

## Build

All targets use CMake presets (CMake ≥ 3.25, Ninja).

```bash
# Host: unit tests, Python parity, C API test (GCC >= 14 or Clang >= 18)
cmake --workflow --preset host
cmake --workflow --preset host-cxx26       # the same suite as C++26
cmake --workflow --preset host-sanitize    # ASan + UBSan

# Targets: Arm GNU Toolchain 14.2 (aarch64-none-elf / arm-none-eabi) on PATH
cmake --workflow --preset zynqmp-a53       # with semihosting: for QEMU / a debugger
cmake --workflow --preset zynqmp-r5f
cmake --workflow --preset cortex-m7
cmake --workflow --preset zynqmp-r5f-board # without semihosting: for a stand-alone board
```

The dev container (`.devcontainer/`) has every toolchain, QEMU and Python installed.

## Run in QEMU

```bash
python3 tools/run_qemu.py zynqmp_a53 build/zynqmp-a53/bsp/zynqmp_a53/ebc_zynqmp_a53.elf
python3 tools/run_qemu.py cortex_m7  build/cortex-m7/bsp/cortex_m7/ebc_cortex_m7.elf
```

Each image prints one CSV line per burst on its UART and ends with
`# verdict: PASS|FAIL`. QEMU's exit code is that verdict, delivered through
semihosting. CI also compares the Cortex-A53 output with `ebc_host_sim --float`
number by number.

## Targets

### Zynq UltraScale+ APU: Cortex-A53 (`zynqmp_a53`)

* Entry `_start` runs at the EL the boot flow provides (EL3 from FSBL/JTAG, EL2/EL1
  behind ATF or in QEMU). It enables FP/SIMD at every level and parks cores 1–3.
* MMU off, so all memory is Device type: built with `-mstrict-align`.
* Image in PS DDR at `0x0010_0000`. Output on PS UART0 at `0xFF00_0000`, 115200 8N1;
  the divisors are computed at compile time with a `static_assert` on the baud error.

### Zynq UltraScale+ RPU: Cortex-R5F (`zynqmp_r5f`)

```
ATCM 0x0000_0000  64 KiB  vectors (ARM state), .text, .rodata, .data load image
BTCM 0x0002_0000  64 KiB  .data, .bss (tables, buffers), SVC/IRQ/ABT/UND stacks
```

* TTC0 counter 1 in interval mode at 2 kHz raises GIC SPI 68 (PL390 at
  `0xF900_0000` / `0xF900_1000`). The IRQ entry saves `r0-r3, r12, lr`, `s0-s15` and
  `FPSCR`, because the handler is C++ and uses the FPU.
* Thumb-2 for C/C++ (`-mcpu=cortex-r5 -mfpu=vfpv3xd -mfloat-abi=hard`). The law is
  single precision; the double-precision half of the R5F's FPU is unused on purpose.
* The pulse path touches TCM only, so its latency does not depend on APU traffic
  on DDR.

### Cortex-M7 (`cortex_m7`)

* Vector table and reset handler in C23 (`startup.c`). The FPU is enabled before
  the first floating-point instruction.
* Reference board: Arm MPS2 AN500 (emulated by QEMU). For an STM32H7, i.MX RT1170 or
  SAM E70, change the two `ORIGIN`s in `link.ld`, the core clock and the UART driver.

## Real-time structure (RPU and M7)

```
trigger IRQ (2 kHz)                         background loop
───────────────────────────────────         ─────────────────────────────────────
pulse 0:  begin_burst()  ◀── acquire table
pulse i:  u = u_ff[i] + PID(E[i-L])         service():
          E = plant/HV path                   learning: u_ff' = Q(u_ff + a·e)  (shadow)
          record(i, E)                        publish shadow  ──▶ next burst
pulse 99: end_burst() ── hand-over ───────▶   KPIs + UART report
20-pulse trigger gap (learning runs here)
```

* The interrupt is O(1): no loop depends on the burst length.
* Learning writes only the unpublished table, and there is at most one pending
  hand-over. The interrupt therefore never reads a table that is being written.
  No locks are needed and interrupts are never masked.
* If learning has not finished when the next burst ends, that burst is dropped from
  learning and counted (`learning_overruns`). The interrupt never waits.

## Status

| | Host | Cortex-A53 | Cortex-R5F | Cortex-M7 |
|---|---|---|---|---|
| Builds with `-Werror` | CI | CI | CI | CI |
| Runs, verdict checked | CI (CTest) | CI (QEMU xlnx-zcu102) | QEMU run experimental | CI (QEMU mps2-an500) |
| Numbers compared with Python / host | parity test | `compare_logs.py` | via shared app code | via shared app code |
| On physical hardware | — | open issue | open issue | open issue |
