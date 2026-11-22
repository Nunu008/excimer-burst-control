"""Compare the KPI logs of two runs of the firmware application line by line.

Used in CI to show that the Cortex-A53 image (in QEMU) computes the same bursts as
the host build of the identical code (``ebc_host_sim --float``). Both print the
same CSV; only libm rounding differs between newlib and the host C library.

    python firmware/tools/compare_logs.py host.csv qemu_a53.log
"""

from __future__ import annotations

import argparse
from pathlib import Path

NUMERIC = ("max_dev_pct", "rms_mj", "std_mj", "mean_abs_ufb")


def parse(path: Path) -> dict[tuple[int, str], dict[str, float]]:
    rows: dict[tuple[int, str], dict[str, float]] = {}
    header: list[str] | None = None
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("$") or line.startswith("qemu"):
            continue
        cells = line.split(",")
        if cells[0] == "burst":
            header = cells
            continue
        if header is None or len(cells) != len(header):
            continue
        record = dict(zip(header, cells, strict=True))
        values = {k: float(record[k]) for k in NUMERIC}
        values["settling_pulse"] = float(record["settling_pulse"])
        rows[(int(record["burst"]), record["strategy"])] = values
    return rows


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--rel", type=float, default=2e-3, help="relative tolerance")
    args = parser.parse_args(argv)

    ref, cand = parse(args.reference), parse(args.candidate)
    if not ref or set(ref) != set(cand):
        print(f"line sets differ: {len(ref)} reference vs {len(cand)} candidate rows")
        return 1

    worst = 0.0
    failures = 0
    for key in sorted(ref):
        for column in NUMERIC:
            a, b = ref[key][column], cand[key][column]
            # Values are printed with 3-4 decimals: allow one unit in the last place.
            tolerance = max(args.rel * abs(a), 1.5e-3 if column == "max_dev_pct" else 1.5e-4)
            worst = max(worst, abs(a - b) / max(abs(a), 1e-9))
            if abs(a - b) > tolerance:
                failures += 1
                print(f"burst {key[0]} {key[1]} {column}: {a} vs {b}")
        if abs(ref[key]["settling_pulse"] - cand[key]["settling_pulse"]) > 2:
            failures += 1
            print(
                f"burst {key[0]} {key[1]} settling: {ref[key]['settling_pulse']} "
                f"vs {cand[key]['settling_pulse']}"
            )

    print(f"{len(ref)} rows compared, worst relative difference {worst:.2e}, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
