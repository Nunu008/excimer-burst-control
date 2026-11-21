"""Control-quality KPIs computed on a single burst."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import ArrayLike

from .filters import moving_average

SETTLING_SMOOTHING_PULSES = 5
"""Settling is judged on a 5-pulse moving average to stay robust against noise."""


@dataclass(frozen=True, slots=True)
class KPIs:
    """Headline numbers describing how well a controller tracked the target."""

    max_deviation_pct: float
    std_mj: float
    rms_mj: float
    settling_pulse: int | None

    @property
    def settled(self) -> bool:
        return self.settling_pulse is not None

    @property
    def rms_pct(self) -> float:
        return float("nan")


def settling_pulse(errors: ArrayLike, band_abs: float) -> int | None:
    """First pulse index after which the smoothed error stays inside the band."""
    smoothed = np.abs(moving_average(errors, SETTLING_SMOOTHING_PULSES))
    outside = smoothed > band_abs
    if not outside.any():
        return 0
    last_violation = int(np.flatnonzero(outside)[-1])
    if last_violation >= smoothed.size - 1:
        return None
    return last_violation + 1


def compute_kpis(energies: ArrayLike, target: float, tolerance_band: float) -> KPIs:
    """Compute deviation, noise and settling KPIs for one burst.

    Args:
        energies: Measured output energy per pulse in mJ.
        target: Target energy in mJ.
        tolerance_band: Relative band (e.g. ``0.02`` for ±2 %) used for settling.
    """
    if target <= 0.0:
        raise ValueError("target must be positive")

    energy = np.asarray(energies, dtype=float).reshape(-1)
    if energy.size == 0:
        raise ValueError("energies must not be empty")

    errors = energy - target
    band_abs = tolerance_band * target
    return KPIs(
        max_deviation_pct=float(np.max(np.abs(errors)) / target * 100.0),
        std_mj=float(np.std(errors)),
        rms_mj=float(np.sqrt(np.mean(errors**2))),
        settling_pulse=settling_pulse(errors, band_abs),
    )
