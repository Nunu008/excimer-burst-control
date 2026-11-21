"""Physical model of the excimer gain medium (the "plant" of the control loop)."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

from .config import PlantConfig


class ExcimerPlant:
    """Static, memoryless plant model sampled pulse by pulse.

    The plant maps an actuator command to output energy using the gas-state
    efficiency of the *current* pulse index.  Stochastic pulse-to-pulse noise is
    injected by the caller, which lets the simulation drive all three control
    strategies with the identical noise realisation and therefore produce a
    genuinely fair comparison.
    """

    def __init__(self, config: PlantConfig) -> None:
        self._config = config

    @property
    def config(self) -> PlantConfig:
        return self._config

    def efficiency(self, pulse_index: int) -> float:
        """Gas-state efficiency factor for ``pulse_index`` (0 = first pulse)."""
        return self._config.efficiency(pulse_index)

    def emit(self, command: float, pulse_index: int, noise_factor: float = 1.0) -> float:
        """Return the output energy in mJ for a single pulse."""
        energy = command * self._config.actuator_gain * self.efficiency(pulse_index) * noise_factor
        return max(0.0, float(energy))

    def noise_factors(self, rng: np.random.Generator, count: int) -> NDArray[np.float64]:
        """Draw multiplicative noise factors (clipped at 0.2 to stay physical)."""
        sigma = self._config.noise_sigma_rel
        if sigma <= 0.0:
            return np.ones(count, dtype=float)
        factors = rng.normal(1.0, sigma, size=count)
        return np.clip(factors, 0.2, None)
