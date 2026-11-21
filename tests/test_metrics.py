"""Tests for the KPI metrics."""

from __future__ import annotations

import numpy as np
import pytest
from src.core.metrics import compute_kpis, settling_pulse

TARGET = 4.0


def test_perfect_tracking_yields_zero_error() -> None:
    kpis = compute_kpis(np.full(100, TARGET), TARGET, 0.02)
    assert kpis.max_deviation_pct == pytest.approx(0.0)
    assert kpis.std_mj == pytest.approx(0.0)
    assert kpis.rms_mj == pytest.approx(0.0)
    assert kpis.settling_pulse == 0


def test_constant_offset_metrics() -> None:
    kpis = compute_kpis(np.full(100, TARGET + 0.2), TARGET, 0.02)
    assert kpis.max_deviation_pct == pytest.approx(5.0)
    assert kpis.std_mj == pytest.approx(0.0)
    assert kpis.rms_mj == pytest.approx(0.2)
    assert kpis.settling_pulse is None


def test_decaying_error_settles_inside_the_band() -> None:
    pulses = np.arange(100)
    energies = TARGET + 0.8 * np.exp(-pulses / 4.0)
    kpis = compute_kpis(energies, TARGET, 0.02)
    assert kpis.max_deviation_pct == pytest.approx(20.0)
    assert kpis.settling_pulse is not None
    assert 10 <= kpis.settling_pulse <= 40


def test_monotonic_drift_never_settles() -> None:
    energies = np.linspace(TARGET, TARGET * 0.7, 100)
    kpis = compute_kpis(energies, TARGET, 0.02)
    assert kpis.settling_pulse is None
    assert kpis.max_deviation_pct == pytest.approx(30.0)


def test_settling_ignores_isolated_noise_spike() -> None:
    errors = np.zeros(50)
    errors[3] = 0.5  # single outlier, far outside the band
    assert settling_pulse(errors, 0.08) == 6  # smoothed 5-pulse window clears it


def test_rms_matches_definition() -> None:
    energies = np.array([3.0, 4.0, 5.0])
    kpis = compute_kpis(energies, TARGET, 0.02)
    assert kpis.rms_mj == pytest.approx(np.sqrt((1 + 0 + 1) / 3))


def test_invalid_inputs_raise() -> None:
    with pytest.raises(ValueError):
        compute_kpis([], TARGET, 0.02)
    with pytest.raises(ValueError):
        compute_kpis([1.0], 0.0, 0.02)
