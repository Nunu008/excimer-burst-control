"""Controllers: open loop, PID feedback and hybrid ILR + PID feedforward."""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np
from numpy.typing import ArrayLike, NDArray

from .config import ILRConfig, PIDConfig
from .filters import moving_average


@dataclass(frozen=True, slots=True)
class Actuation:
    """One pulse of control action, split into its physical contributions."""

    command: float
    feedforward: float
    feedback: float


class PIDController:
    """Discrete PID controller with integral anti-windup and output clamping."""

    def __init__(self, config: PIDConfig) -> None:
        self._config = config
        self._integral = 0.0
        self._previous_error = 0.0

    @property
    def config(self) -> PIDConfig:
        return self._config

    @property
    def integral(self) -> float:
        return self._integral

    def reset(self) -> None:
        self._integral = 0.0
        self._previous_error = 0.0

    def update(self, error: float) -> float:
        """Advance the controller by one pulse and return the feedback term."""
        cfg = self._config
        self._integral = max(-cfg.integral_limit, min(cfg.integral_limit, self._integral + error))
        derivative = error - self._previous_error
        self._previous_error = error
        output = cfg.kp * error + cfg.ki * self._integral + cfg.kd * derivative
        return cfg.clamped(output)

    def act(self, target: float, measurement: float, base_command: float = 0.0) -> Actuation:
        """Return the full actuation for one pulse (feedforward baseline + PID)."""
        feedback = self.update(target - measurement)
        command = max(0.0, base_command + feedback)
        return Actuation(command=command, feedforward=base_command, feedback=feedback)


class IterativeLearningController:
    """Iterative Learning Control on a per-pulse feedforward matrix.

    The learned quantity is the command profile ``u_ff[i]`` for every pulse index
    ``i`` inside the burst.  After each burst the profile is corrected by the
    observed error::

        u_ff,k+1 = Q(u_ff,k + alpha * e_k)

    where ``Q`` is a centred moving average (the Q-filter). Applying ``Q`` to the
    whole update, not only to the error, is the robust ILC form: with the PID and
    its dead time inside the loop, ``||Q (I - alpha G)||_2 < 1`` holds for every
    preset, so the learning converges monotonically. Filtering only the error
    (``u_ff + alpha * Q(e)``) has a spectral radius above one and diverges after
    a few dozen bursts (see ADR 0004 and ``scripts/ilc_stability.py``).
    """

    def __init__(self, config: ILRConfig, initial_command: float, pulses_per_burst: int) -> None:
        config.validate(pulses_per_burst)
        self._config = config
        self._initial_command = initial_command
        self._pulses = pulses_per_burst
        self._matrix = np.full(config.grid_shape, initial_command, dtype=float)
        self._bursts_learned = 0

    @property
    def config(self) -> ILRConfig:
        return self._config

    @property
    def matrix(self) -> NDArray[np.float64]:
        """Copy of the feedforward matrix, shaped ``grid_shape``."""
        return self._matrix.copy()

    @property
    def profile(self) -> NDArray[np.float64]:
        """Flat per-pulse feedforward command (row-major flattening of the matrix)."""
        return self._matrix.reshape(-1).copy()

    @property
    def bursts_learned(self) -> int:
        return self._bursts_learned

    def command(self, pulse_index: int) -> float:
        rows, cols = self._config.grid_shape
        row, col = divmod(pulse_index, cols)
        if row >= rows:
            raise IndexError(f"pulse_index {pulse_index} outside the learned matrix")
        return float(self._matrix[row, col])

    def reset(self) -> None:
        self._matrix.fill(self._initial_command)
        self._bursts_learned = 0

    def update(self, errors: ArrayLike) -> float:
        """Apply one learning step from the observed errors of the last burst.

        Args:
            errors: Error signal ``target - measured`` for every pulse of the burst.

        Returns:
            The largest absolute correction applied in this update.
        """
        error_array = np.asarray(errors, dtype=float).reshape(-1)
        if error_array.size != self._pulses:
            raise ValueError(f"expected {self._pulses} errors, got {error_array.size}")

        current = self._matrix.reshape(-1)
        proposed = current + self._config.learning_rate * error_array
        updated = moving_average(proposed, self._config.smoothing_depth)
        delta = updated - current
        self._matrix = np.maximum(updated, 0.0).reshape(self._config.grid_shape)
        self._bursts_learned += 1
        return float(np.max(np.abs(delta))) if delta.size else 0.0


@dataclass(slots=True)
class HybridController:
    """Feedforward ILR profile with a PID trimming the residual error."""

    learning: IterativeLearningController
    pid: PIDController = field(default_factory=lambda: PIDController(PIDConfig()))
    command_limit: float = 12.0

    def reset(self) -> None:
        self.learning.reset()
        self.pid.reset()

    def reset_feedback(self) -> None:
        """Clear the PID state at a burst boundary.

        The repetitive part of the disturbance is the job of the learned table,
        so a cross-burst integral would only add a constant offset that the
        learner can never absorb (the feedforward would converge 4 % low while
        the integrator silently carried the rest).  Resetting the integrator per
        burst keeps the split between ``u_ff`` and ``u_fb`` physically
        meaningful: in steady state the table delivers the whole command and the
        PID only trims the non-repetitive noise.
        """
        self.pid.reset()

    def act(self, pulse_index: int, target: float, measurement: float) -> Actuation:
        feedforward = self.learning.command(pulse_index)
        feedback = self.pid.update(target - measurement)
        command = min(self.command_limit, max(0.0, feedforward + feedback))
        return Actuation(command=command, feedforward=feedforward, feedback=feedback)
