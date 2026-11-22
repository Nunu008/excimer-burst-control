"""Lifted-system convergence analysis of the hybrid ILC + PID loop.

Within one burst the hybrid loop is linear around its trajectory (ignoring the
actuator clamps): a perturbation ``du_ff`` of the feedforward table produces an
energy perturbation ``dE = G du_ff``, where ``G`` (pulses x pulses) contains the
plant efficiency *and* the in-burst PID reaction through the Timelog dead time.
``G`` is obtained column by column by simulating unit perturbations.

One learning step then propagates the table error as

* ``M = I - alpha * Q G``    for the error-filtered law ``u + alpha Q(e)``
* ``M = Q (I - alpha G)``    for the robust law ``Q(u + alpha e)`` (Bristow,
  Tharayil and Alleyne, IEEE Control Systems Magazine, 2006)

The learning converges asymptotically iff the spectral radius of ``M`` is below
one, and monotonically (no learning transient) if its 2-norm is below one.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray

from .config import SimulationConfig
from .filters import moving_average


@dataclass(frozen=True, slots=True)
class Contraction:
    """Convergence figures of one learning law."""

    spectral_radius: float
    norm_2: float

    @property
    def converges(self) -> bool:
        return self.spectral_radius < 1.0

    @property
    def monotone(self) -> bool:
        return self.norm_2 < 1.0


def lifted_closed_loop(config: SimulationConfig) -> NDArray[np.float64]:
    """Return ``G = dE / du_ff`` of one hybrid burst (PID reset at burst start)."""
    pulses = config.pulses_per_burst
    eta = np.array([config.plant.efficiency(i) * config.plant.actuator_gain for i in range(pulses)])
    delay = config.measurement_delay_pulses
    pid = config.pid
    lifted = np.zeros((pulses, pulses))
    for column in range(pulses):
        energy = np.zeros(pulses)
        integral = 0.0
        previous = 0.0
        for i in range(pulses):
            error = -energy[i - delay] if i >= delay else 0.0
            integral += error
            derivative = error - previous
            previous = error
            feedforward = 1.0 if i == column else 0.0
            feedback = pid.kp * error + pid.ki * integral + pid.kd * derivative
            energy[i] = eta[i] * (feedforward + feedback)
        lifted[:, column] = energy
    return lifted


def q_filter_matrix(pulses: int, depth: int) -> NDArray[np.float64]:
    """The Q-filter (centred moving average with edge padding) as a matrix."""
    identity = np.eye(pulses)
    return np.column_stack([moving_average(identity[:, j], depth) for j in range(pulses)])


def _contraction(matrix: NDArray[np.float64]) -> Contraction:
    return Contraction(
        spectral_radius=float(np.max(np.abs(np.linalg.eigvals(matrix)))),
        norm_2=float(np.linalg.norm(matrix, 2)),
    )


def learning_contraction(config: SimulationConfig) -> Contraction:
    """Convergence of the implemented (robust) law ``Q(u_ff + alpha e)``."""
    g = lifted_closed_loop(config)
    q = q_filter_matrix(config.pulses_per_burst, config.ilr.smoothing_depth)
    return _contraction(q @ (np.eye(g.shape[0]) - config.ilr.learning_rate * g))


def error_filtered_contraction(config: SimulationConfig) -> Contraction:
    """Convergence of the superseded law ``u_ff + alpha Q(e)`` (for comparison)."""
    g = lifted_closed_loop(config)
    q = q_filter_matrix(config.pulses_per_burst, config.ilr.smoothing_depth)
    return _contraction(np.eye(g.shape[0]) - config.ilr.learning_rate * q @ g)
