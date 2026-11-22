"""Run a firmware image in QEMU like a test: capture the UART, return its verdict.

The images report PASS/FAIL as a semihosting exit code, so QEMU's own exit code is
the test result. A hung image is killed after ``--timeout`` seconds and fails.

    python firmware/tools/run_qemu.py zynqmp_a53 build/zynqmp-a53/bsp/zynqmp_a53/ebc_zynqmp_a53.elf
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

SEMIHOSTING = ["-semihosting-config", "enable=on,target=native"]
SERIAL = ["-display", "none", "-monitor", "none", "-serial", "stdio"]

MACHINES: dict[str, list[str]] = {
    # Zynq UltraScale+ ZCU102: APU core 0. PS UART0 is the first serial port.
    "zynqmp_a53": ["qemu-system-aarch64", "-M", "xlnx-zcu102", "-m", "2G"],
    # Same SoC, RPU core 0 (CPU index 4 after the four A53 cores).
    "zynqmp_r5f": ["qemu-system-aarch64", "-M", "xlnx-zcu102", "-m", "2G"],
    # Arm MPS2 with the AN500 Cortex-M7 FPGA image.
    "cortex_m7": ["qemu-system-arm", "-M", "mps2-an500"],
}

LOADER: dict[str, list[str]] = {
    "zynqmp_a53": ["-device", "loader,file={elf},cpu-num=0"],
    "zynqmp_r5f": ["-device", "loader,file={elf},cpu-num=4"],
    "cortex_m7": ["-kernel", "{elf}"],
}


def command(target: str, elf: Path) -> list[str]:
    loader = [part.format(elf=elf) for part in LOADER[target]]
    return [*MACHINES[target], *SERIAL, *SEMIHOSTING, *loader]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("target", choices=sorted(MACHINES))
    parser.add_argument("elf", type=Path)
    parser.add_argument("--log", type=Path, help="write the UART output here")
    parser.add_argument("--timeout", type=float, default=90.0)
    args = parser.parse_args(argv)

    cmd = command(args.target, args.elf)
    print("$ " + " ".join(cmd), flush=True)
    try:
        result = subprocess.run(cmd, capture_output=True, timeout=args.timeout, check=False)
    except FileNotFoundError:
        print(f"{cmd[0]} not found: install QEMU (apt install qemu-system-arm)", file=sys.stderr)
        return 127
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or b"").decode(errors="replace").replace("\r", "")
        print(output)
        print(f"TIMEOUT after {args.timeout:.0f} s", file=sys.stderr)
        return 124

    output = result.stdout.decode(errors="replace").replace("\r", "")
    print(output, end="")
    if result.stderr:
        print(result.stderr.decode(errors="replace"), file=sys.stderr, end="")
    if args.log:
        args.log.write_text(output, encoding="utf-8")
    print(f"qemu exit code: {result.returncode}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
