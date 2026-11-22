"""Export golden vectors from the Python reference model for the C++ parity test.

For every preset the script runs the Python ``SimulationEngine`` and records, per
burst and pulse, the noise factor that was drawn and the resulting energies and
actuator split of all three strategies, plus the per-burst KPIs. The C++ test
(``firmware/tests/test_python_parity.cpp``) injects the same noise into the firmware
core and must reproduce every number.

Usage::

    python scripts/export_golden.py            # write firmware/tests/golden/
    python scripts/export_golden.py --check    # exit 1 if the files are stale
"""

from __future__ import annotations

import argparse
import io
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from src.core.config import apply_preset  # noqa: E402
from src.core.simulation import MODE_HYBRID, MODE_OPEN_LOOP, MODE_PID, SimulationEngine  # noqa: E402

GOLDEN_DIR = ROOT / "firmware" / "tests" / "golden"
PRESETS = ("standard", "noise", "latency")
BURSTS = 15
MODES = (MODE_OPEN_LOOP, MODE_PID, MODE_HYBRID)


def _fmt(value: float) -> str:
    return repr(float(value))  # shortest round-trip representation


def render(preset: str, bursts: int = BURSTS) -> tuple[str, str]:
    """Return the (pulses CSV, KPI CSV) contents for one preset."""
    engine = SimulationEngine(apply_preset(preset))

    recorded: list[np.ndarray] = []
    draw = engine.plant.noise_factors

    def recording_draw(rng: np.random.Generator, count: int) -> np.ndarray:
        factors = draw(rng, count)
        recorded.append(factors.copy())
        return factors

    engine.plant.noise_factors = recording_draw  # type: ignore[method-assign]

    pulses = io.StringIO()
    pulses.write("burst,pulse,noise,e_open_loop,e_pid,e_hybrid,u_pid,u_ff_hybrid,u_fb_hybrid\n")
    kpis = io.StringIO()
    kpis.write("burst,mode,max_deviation_pct,std_mj,rms_mj,settling_pulse\n")

    for burst in range(bursts):
        snapshot = engine.step()
        noise = recorded[burst]
        ol, pid, hyb = (snapshot.result(m) for m in MODES)
        for i in range(noise.size):
            row = (
                noise[i],
                ol.energy_mj[i],
                pid.energy_mj[i],
                hyb.energy_mj[i],
                pid.command[i],
                hyb.feedforward[i],
                hyb.feedback[i],
            )
            pulses.write(f"{burst},{i}," + ",".join(_fmt(v) for v in row) + "\n")
        for mode in MODES:
            k = snapshot.result(mode).kpis
            settling = -1 if k.settling_pulse is None else k.settling_pulse
            kpis.write(
                f"{burst},{mode},{_fmt(k.max_deviation_pct)},{_fmt(k.std_mj)},"
                f"{_fmt(k.rms_mj)},{settling}\n"
            )
    return pulses.getvalue(), kpis.getvalue()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="verify instead of writing")
    args = parser.parse_args(argv)

    stale: list[str] = []
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    for preset in PRESETS:
        for suffix, content in zip(("pulses", "kpis"), render(preset), strict=True):
            path = GOLDEN_DIR / f"{preset}_{suffix}.csv"
            if args.check:
                current = path.read_text(encoding="utf-8") if path.exists() else ""
                if current != content:
                    stale.append(path.name)
            else:
                path.write_text(content, encoding="utf-8", newline="\n")
                print(f"wrote {path.relative_to(ROOT)}")

    if stale:
        print("stale golden vectors: " + ", ".join(stale), file=sys.stderr)
        print("run: python scripts/export_golden.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
