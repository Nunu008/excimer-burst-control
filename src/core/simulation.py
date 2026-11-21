"""Simulation engine running the three control strategies in lock-step."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from .config import SimulationConfig
from .controllers import HybridController, IterativeLearningController, PIDController
from .metrics import KPIs, compute_kpis
from .plant import ExcimerPlant

MODE_OPEN_LOOP = "hv_open_loop"
MODE_PID = "pid_only"
MODE_HYBRID = "hybrid_ilr_pid"

MODE_LABELS: dict[str, str] = {
    MODE_OPEN_LOOP: "HV constant (open loop)",
    MODE_PID: "PID only",
    MODE_HYBRID: "Hybrid ILR + PID",
}

MODE_COLORS: dict[str, str] = {
    MODE_OPEN_LOOP: "#e0a33e",
    MODE_PID: "#d95f5f",
    MODE_HYBRID: "#4fd1c5",
}


@dataclass(frozen=True, slots=True)
class BurstResult:
    """All signals of one control strategy for one burst."""

    mode: str
    energy_mj: NDArray[np.float64]
    command: NDArray[np.float64]
    feedforward: NDArray[np.float64]
    feedback: NDArray[np.float64]
    kpis: KPIs


@dataclass(frozen=True, slots=True)
class BurstSnapshot:
    """Complete result set of a single simulated burst."""

    index: int
    target_mj: float
    results: dict[str, BurstResult]
    matrix: NDArray[np.float64]
    learned_bursts: int

    def result(self, mode: str) -> BurstResult:
        return self.results[mode]


class SimulationEngine:
    """Runs the HVAC-command simulation burst by burst.

    Every burst drives all three strategies with the *same* noise realisation so
    the performance comparison is not biased by different random draws.
    """

    def __init__(self, config: SimulationConfig) -> None:
        self._config = config
        self._plant = ExcimerPlant(config.plant)
        self._rng = np.random.default_rng(config.seed)
        self._pid_only = PIDController(config.pid)
        self._hybrid = HybridController(
            learning=IterativeLearningController(
                config=config.ilr,
                initial_command=config.plant.nominal_command,
                pulses_per_burst=config.pulses_per_burst,
            ),
            pid=PIDController(config.pid),
        )
        self._burst_index = 0
        self._history: list[BurstSnapshot] = []

    # ------------------------------------------------------------------ state

    @property
    def config(self) -> SimulationConfig:
        return self._config

    @property
    def plant(self) -> ExcimerPlant:
        return self._plant

    @property
    def matrix(self) -> NDArray[np.float64]:
        return self._hybrid.learning.matrix

    @property
    def learned_bursts(self) -> int:
        return self._hybrid.learning.bursts_learned

    @property
    def bursts_completed(self) -> int:
        return self._burst_index

    @property
    def history(self) -> list[BurstSnapshot]:
        return list(self._history)

    def reset(self) -> None:
        """Restore the initial state (same seed, freshly initialised controllers)."""
        cfg = self._config
        self._rng = np.random.default_rng(cfg.seed)
        self._pid_only.reset()
        self._hybrid.reset()
        self._burst_index = 0
        self._history.clear()

    # ------------------------------------------------------------- simulation

    def step(self) -> BurstSnapshot:
        """Simulate exactly one burst and return its snapshot."""
        cfg = self._config
        plant_cfg = cfg.plant
        pulses = cfg.pulses_per_burst
        target = plant_cfg.target_energy_mj

        noise = self._plant.noise_factors(self._rng, pulses)
        open_loop = self._run_open_loop(noise)
        pid_only = self._run_pid_only(noise)
        hybrid = self._run_hybrid(noise)

        self._hybrid.learning.update(target - hybrid.energy_mj)

        snapshot = BurstSnapshot(
            index=self._burst_index,
            target_mj=target,
            results={
                MODE_OPEN_LOOP: open_loop,
                MODE_PID: pid_only,
                MODE_HYBRID: hybrid,
            },
            matrix=self._hybrid.learning.matrix,
            learned_bursts=self._hybrid.learning.bursts_learned,
        )
        self._burst_index += 1
        self._history.append(snapshot)
        return snapshot

    def run(self, bursts: int) -> list[BurstSnapshot]:
        """Simulate ``bursts`` consecutive bursts and return their snapshots."""
        if bursts < 0:
            raise ValueError("bursts must be non-negative")
        return [self.step() for _ in range(bursts)]

    # ----------------------------------------------------------- mode runners

    def _run_open_loop(self, noise: NDArray[np.float64]) -> BurstResult:
        cfg = self._config
        command_value = cfg.plant.nominal_command
        pulses = cfg.pulses_per_burst
        energy = np.empty(pulses, dtype=float)
        for i in range(pulses):
            energy[i] = self._plant.emit(command_value, i, noise[i])
        command = np.full(pulses, command_value, dtype=float)
        return BurstResult(
            mode=MODE_OPEN_LOOP,
            energy_mj=energy,
            command=command,
            feedforward=command.copy(),
            feedback=np.zeros(pulses, dtype=float),
            kpis=self._kpis(energy),
        )

    def _run_pid_only(self, noise: NDArray[np.float64]) -> BurstResult:
        cfg = self._config
        pulses = cfg.pulses_per_burst
        target = cfg.plant.target_energy_mj
        base = cfg.plant.nominal_command
        energy = np.empty(pulses, dtype=float)
        command = np.empty(pulses, dtype=float)
        feedback = np.empty(pulses, dtype=float)

        for i in range(pulses):
            measurement = self._measurement(energy, i)
            actuation = self._pid_only.act(target, measurement, base)
            command[i] = actuation.command
            feedback[i] = actuation.feedback
            energy[i] = self._plant.emit(actuation.command, i, noise[i])

        return BurstResult(
            mode=MODE_PID,
            energy_mj=energy,
            command=command,
            feedforward=np.full(pulses, base, dtype=float),
            feedback=feedback,
            kpis=self._kpis(energy),
        )

    def _run_hybrid(self, noise: NDArray[np.float64]) -> BurstResult:
        cfg = self._config
        pulses = cfg.pulses_per_burst
        target = cfg.plant.target_energy_mj
        self._hybrid.reset_feedback()
        energy = np.empty(pulses, dtype=float)
        command = np.empty(pulses, dtype=float)
        feedforward = np.empty(pulses, dtype=float)
        feedback = np.empty(pulses, dtype=float)

        for i in range(pulses):
            measurement = self._measurement(energy, i)
            actuation = self._hybrid.act(i, target, measurement)
            command[i] = actuation.command
            feedforward[i] = actuation.feedforward
            feedback[i] = actuation.feedback
            energy[i] = self._plant.emit(actuation.command, i, noise[i])

        return BurstResult(
            mode=MODE_HYBRID,
            energy_mj=energy,
            command=command,
            feedforward=feedforward,
            feedback=feedback,
            kpis=self._kpis(energy),
        )

    # ----------------------------------------------------------------- helpers

    def _measurement(self, energy: NDArray[np.float64], pulse_index: int) -> float:
        """Timelog delay: pulse ``i`` sees the energy measured ``delay`` pulses ago."""
        delayed_index = pulse_index - self._config.measurement_delay_pulses
        if delayed_index < 0:
            return self._config.plant.target_energy_mj
        return float(energy[delayed_index])

    def _kpis(self, energy: NDArray[np.float64]) -> KPIs:
        cfg = self._config
        return compute_kpis(energy, cfg.plant.target_energy_mj, cfg.tolerance_band)
