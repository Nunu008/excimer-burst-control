"""Typed configuration objects for the excimer burst-energy simulation.

All physical quantities are SI-derived (mJ, Hz, pulses) and every parameter that
influences the simulation is declared here so that the UI, the engine and the
tests share exactly one source of truth.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field, replace
from typing import Final

DEFAULT_TOLERANCE_BAND: Final[float] = 0.02
"""Relative tolerance band (±2 %) used for settling-time evaluation."""


@dataclass(frozen=True, slots=True)
class PlantConfig:
    """Physical model of the laser gain medium during one burst.

    The model is deliberately linear in the actuator coordinate: the command
    ``u`` is expressed in "mJ-equivalent" units around the operating point, so
    that ``energy = u * efficiency`` holds exactly.  This is the standard
    small-signal linearisation of the HV-to-energy characteristic of an excimer
    laser and keeps the ILR update law dimensionally consistent.
    """

    target_energy_mj: float = 4.0
    pulses_per_burst: int = 100
    repetition_rate_hz: float = 2000.0
    decay_rate: float = 0.004
    """Per-pulse exponential gas degradation, ``exp(-decay_rate * i)``."""
    startup_transient_depth: float = 0.15
    """Depth of the fast burst-start dip (fraction of nominal energy)."""
    startup_transient_tau: float = 3.0
    """Time constant (in pulses) of the burst-start dip."""
    noise_sigma_rel: float = 0.01
    """Relative pulse-to-pulse energy noise (1 sigma)."""
    actuator_gain: float = 1.0
    """Static gain in mJ per command unit."""

    def efficiency(self, pulse_index: int) -> float:
        """Gas-state efficiency factor at a given pulse index within the burst."""
        degradation = math.exp(-self.decay_rate * pulse_index)
        transient = 1.0 - self.startup_transient_depth * math.exp(
            -pulse_index / self.startup_transient_tau
        )
        return degradation * transient

    @property
    def nominal_command(self) -> float:
        """Open-loop command that hits the target on the very first pulse."""
        return self.target_energy_mj / (self.actuator_gain * self.efficiency(0))

    @property
    def pulse_period_s(self) -> float:
        return 1.0 / self.repetition_rate_hz


@dataclass(frozen=True, slots=True)
class PIDConfig:
    """Discrete PID gains operating on the pulse-to-pulse error signal."""

    kp: float = 0.55
    ki: float = 0.06
    kd: float = 0.02
    integral_limit: float = 40.0
    output_limit: float = 6.0

    def clamped(self, value: float) -> float:
        return max(-self.output_limit, min(self.output_limit, value))


@dataclass(frozen=True, slots=True)
class ILRConfig:
    """Iterative Learning Control parameters (feedforward matrix learning)."""

    learning_rate: float = 0.6
    """Learning factor ``alpha`` of the update law."""
    smoothing_depth: int = 3
    """Q-filter depth: centred moving average over this many pulses."""
    grid_shape: tuple[int, int] = (10, 10)
    """Shape of the feedforward matrix; rows * cols must equal the burst length."""

    def validate(self, pulses_per_burst: int) -> None:
        if self.smoothing_depth < 1:
            raise ValueError("smoothing_depth must be >= 1")
        if not 0.0 < self.learning_rate <= 1.0:
            raise ValueError("learning_rate must be in (0, 1]")
        rows, cols = self.grid_shape
        if rows * cols != pulses_per_burst:
            raise ValueError(
                f"grid_shape {self.grid_shape} does not match {pulses_per_burst} pulses"
            )


@dataclass(frozen=True, slots=True)
class SimulationConfig:
    """Complete, immutable description of one simulation scenario."""

    plant: PlantConfig = field(default_factory=PlantConfig)
    pid: PIDConfig = field(default_factory=PIDConfig)
    ilr: ILRConfig = field(default_factory=ILRConfig)
    measurement_delay_pulses: int = 2
    """Timelog dead time: measurement of pulse ``i`` is available at ``i + delay``."""
    tolerance_band: float = DEFAULT_TOLERANCE_BAND
    seed: int = 42

    def __post_init__(self) -> None:
        if self.measurement_delay_pulses < 1:
            raise ValueError("measurement_delay_pulses must be >= 1")
        if not 0.0 < self.tolerance_band < 0.5:
            raise ValueError("tolerance_band must be in (0, 0.5)")
        self.ilr.validate(self.plant.pulses_per_burst)

    @property
    def pulses_per_burst(self) -> int:
        return self.plant.pulses_per_burst


PRESETS: Final[dict[str, str]] = {
    "standard": "Standard 2 kHz",
    "noise": "Strong gas noise",
    "latency": "High latency (3 pulses)",
}


def apply_preset(name: str) -> SimulationConfig:
    """Return a fresh configuration for a named UI preset."""
    base = SimulationConfig()
    if name == "standard":
        return base
    if name == "noise":
        return replace(
            base,
            plant=replace(
                base.plant,
                noise_sigma_rel=0.04,
                decay_rate=0.006,
                startup_transient_depth=0.22,
            ),
        )
    if name == "latency":
        # Same gains on purpose: only the dead time changes, so the degradation
        # is unambiguously attributable to the Timelog latency.  Raising Kp in
        # the UI on top of this preset drives the loop into visible ringing.
        return replace(base, measurement_delay_pulses=3)
    raise ValueError(f"unknown preset: {name!r}")
