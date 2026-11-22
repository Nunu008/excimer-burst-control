"""Convergence of the learning law: analysis and long-horizon simulation agree."""

from __future__ import annotations

import pytest
from src.core.config import apply_preset
from src.core.simulation import MODE_HYBRID, SimulationEngine
from src.core.stability import (
    error_filtered_contraction,
    learning_contraction,
    lifted_closed_loop,
)

PRESETS = ("standard", "noise", "latency")


@pytest.mark.parametrize("preset", PRESETS)
def test_implemented_law_is_a_contraction(preset: str) -> None:
    contraction = learning_contraction(apply_preset(preset))
    assert contraction.converges
    assert contraction.monotone  # ||Q (I - alpha G)||_2 < 1: no learning transient


@pytest.mark.parametrize("preset", PRESETS)
def test_error_filtered_law_would_diverge(preset: str) -> None:
    # Documents why the update law was changed (ADR 0004): filtering only the error
    # leaves an unstable high-frequency mode once the PID acts inside the burst.
    assert error_filtered_contraction(apply_preset(preset)).spectral_radius > 1.0


def test_lifted_map_has_the_plant_on_its_diagonal() -> None:
    config = apply_preset("standard")
    g = lifted_closed_loop(config)
    for i in (0, 10, 99):
        assert g[i, i] == pytest.approx(config.plant.efficiency(i))
    # The PID only reacts after the dead time: G is lower triangular.
    assert abs(g[0, 5]) == 0.0


@pytest.mark.parametrize(
    ("preset", "rms_limit"), [("standard", 0.08), ("noise", 0.25), ("latency", 0.08)]
)
def test_hybrid_stays_converged_over_200_bursts(preset: str, rms_limit: float) -> None:
    snapshots = SimulationEngine(apply_preset(preset)).run(200)
    worst = max(s.result(MODE_HYBRID).kpis.rms_mj for s in snapshots[15:])
    assert worst < rms_limit
