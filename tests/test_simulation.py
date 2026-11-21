"""Integration tests for the simulation engine and its learning behaviour."""

from __future__ import annotations

from dataclasses import replace

import numpy as np
import pytest
from src.core.config import SimulationConfig, apply_preset
from src.core.simulation import (
    MODE_HYBRID,
    MODE_OPEN_LOOP,
    MODE_PID,
    SimulationEngine,
)


def run_bursts(config: SimulationConfig, bursts: int):
    engine = SimulationEngine(config)
    return engine, [engine.step() for _ in range(bursts)]


def test_every_burst_contains_all_three_modes() -> None:
    engine = SimulationEngine(SimulationConfig())
    snapshot = engine.step()

    assert set(snapshot.results) == {MODE_OPEN_LOOP, MODE_PID, MODE_HYBRID}
    for result in snapshot.results.values():
        assert result.energy_mj.size == 100
        assert np.all(np.isfinite(result.energy_mj))


def test_noise_free_open_loop_follows_the_efficiency_profile() -> None:
    config = SimulationConfig(plant=SimulationConfig().plant.__class__(noise_sigma_rel=0.0))
    engine = SimulationEngine(config)
    snapshot = engine.step()

    result = snapshot.result(MODE_OPEN_LOOP)
    nominal = config.plant.nominal_command
    for index in (0, 10, 50, 99):
        expected = nominal * config.plant.efficiency(index)
        assert result.energy_mj[index] == pytest.approx(expected, rel=1e-9)


def test_dead_time_holds_feedback_off_for_the_first_pulses() -> None:
    config = SimulationConfig(
        measurement_delay_pulses=3,
        plant=SimulationConfig().plant.__class__(noise_sigma_rel=0.0),
    )
    snapshot = SimulationEngine(config).step()
    nominal = config.plant.nominal_command

    for index in range(3):
        assert snapshot.result(MODE_PID).command[index] == pytest.approx(nominal)
        assert snapshot.result(MODE_HYBRID).feedback[index] == pytest.approx(0.0)


def test_pid_energy_stays_bounded_over_many_bursts() -> None:
    _, snapshots = run_bursts(SimulationConfig(), 20)
    peaks = [float(np.max(s.result(MODE_PID).energy_mj)) for s in snapshots]
    assert max(peaks) < 2.0 * 4.0
    assert min(peaks) > 0.5 * 4.0


def test_hybrid_learning_converges_and_beats_open_loop() -> None:
    _, snapshots = run_bursts(SimulationConfig(), 15)

    first = snapshots[0]
    last = snapshots[-1]

    first_hybrid_rms = first.result(MODE_HYBRID).kpis.rms_mj
    last_hybrid = last.result(MODE_HYBRID).kpis
    open_loop = last.result(MODE_OPEN_LOOP).kpis

    assert last_hybrid.rms_mj < 0.08
    assert last_hybrid.rms_mj < 0.4 * first_hybrid_rms
    assert last_hybrid.rms_mj < 0.4 * open_loop.rms_mj


def test_hybrid_beats_pure_pid_after_convergence() -> None:
    _, snapshots = run_bursts(SimulationConfig(), 15)
    last = snapshots[-1]

    assert last.result(MODE_HYBRID).kpis.rms_mj < 0.5 * last.result(MODE_PID).kpis.rms_mj
    assert last.result(MODE_HYBRID).kpis.max_deviation_pct < (
        last.result(MODE_PID).kpis.max_deviation_pct
    )


def test_learned_profile_approaches_inverse_efficiency() -> None:
    base = SimulationConfig()
    config = replace(base, plant=replace(base.plant, noise_sigma_rel=0.0))
    engine = SimulationEngine(config)
    for _ in range(25):
        engine.step()

    profile = engine.matrix.reshape(-1)
    target = config.plant.target_energy_mj
    for index in (0, 1, 25, 60, 99):
        expected = target / config.plant.efficiency(index)
        assert profile[index] == pytest.approx(expected, rel=0.01)


def test_feedback_share_decays_while_the_table_takes_over() -> None:
    _, snapshots = run_bursts(SimulationConfig(), 15)
    first = float(np.mean(np.abs(snapshots[0].result(MODE_HYBRID).feedback)))
    last = float(np.mean(np.abs(snapshots[-1].result(MODE_HYBRID).feedback)))

    assert last < 0.2 * first
    assert last < 0.05


def test_noise_keeps_the_learned_profile_within_a_bounded_band() -> None:
    engine = SimulationEngine(SimulationConfig())
    for _ in range(30):
        engine.step()

    profile = engine.matrix.reshape(-1)
    ideal = 4.0 / np.array([SimulationConfig().plant.efficiency(i) for i in range(100)])
    assert float(np.max(np.abs(profile - ideal) / ideal)) < 0.12


def test_learning_is_monotone_in_the_first_bursts() -> None:
    _, snapshots = run_bursts(SimulationConfig(), 8)
    rms = [s.result(MODE_HYBRID).kpis.rms_mj for s in snapshots]
    assert rms[7] < rms[3] < rms[1]


def test_reset_restores_the_initial_state() -> None:
    engine = SimulationEngine(SimulationConfig())
    engine.run(5)
    engine.reset()

    assert engine.bursts_completed == 0
    assert engine.learned_bursts == 0
    assert np.allclose(engine.matrix, SimulationConfig().plant.nominal_command)


def test_seed_makes_runs_reproducible() -> None:
    first = SimulationEngine(SimulationConfig()).step()
    second = SimulationEngine(SimulationConfig()).step()
    assert np.allclose(first.result(MODE_PID).energy_mj, second.result(MODE_PID).energy_mj)


def test_latency_preset_keeps_everything_stable() -> None:
    _, snapshots = run_bursts(apply_preset("latency"), 10)
    last = snapshots[-1]
    assert last.result(MODE_HYBRID).kpis.rms_mj < 0.2
    assert np.all(np.isfinite(last.result(MODE_PID).energy_mj))


def test_noise_preset_degrades_pid_more_than_hybrid() -> None:
    _, snapshots = run_bursts(apply_preset("noise"), 12)
    last = snapshots[-1]
    assert last.result(MODE_HYBRID).kpis.rms_mj < 0.6 * last.result(MODE_PID).kpis.rms_mj
