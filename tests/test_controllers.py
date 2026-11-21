"""Unit tests for PID and Iterative Learning Control."""

from __future__ import annotations

import numpy as np
import pytest
from src.core.config import ILRConfig, PIDConfig
from src.core.controllers import HybridController, IterativeLearningController, PIDController


class TestPID:
    def test_zero_error_produces_no_action(self) -> None:
        pid = PIDController(PIDConfig())
        assert pid.update(0.0) == pytest.approx(0.0)

    def test_proportional_term_dominates_for_first_sample(self) -> None:
        pid = PIDController(PIDConfig(kp=0.5, ki=0.0, kd=0.0))
        assert pid.update(2.0) == pytest.approx(1.0)

    def test_integral_is_clamped_to_anti_windup_limit(self) -> None:
        config = PIDConfig(kp=0.0, ki=1.0, kd=0.0, integral_limit=5.0)
        pid = PIDController(config)
        for _ in range(100):
            pid.update(1.0)
        assert pid.integral == pytest.approx(5.0)

    def test_output_is_clamped(self) -> None:
        pid = PIDController(PIDConfig(kp=100.0, ki=0.0, kd=0.0, output_limit=3.0))
        assert pid.update(10.0) == pytest.approx(3.0)

    def test_negative_error_reduces_command(self) -> None:
        pid = PIDController(PIDConfig(kp=0.5, ki=0.0, kd=0.0))
        actuation = pid.act(target=4.0, measurement=4.4, base_command=4.0)
        assert actuation.command < 4.0
        assert actuation.feedback < 0.0

    def test_reset_clears_state(self) -> None:
        pid = PIDController(PIDConfig())
        for _ in range(5):
            pid.update(1.0)
        pid.reset()
        assert pid.integral == 0.0
        assert pid.update(0.0) == pytest.approx(0.0)


class TestILR:
    def test_matrix_starts_flat(self) -> None:
        ilr = IterativeLearningController(ILRConfig(), initial_command=4.5, pulses_per_burst=100)
        assert ilr.matrix.shape == (10, 10)
        assert np.allclose(ilr.matrix, 4.5)
        assert ilr.bursts_learned == 0

    def test_profile_matches_matrix_row_major(self) -> None:
        ilr = IterativeLearningController(ILRConfig(), initial_command=4.0, pulses_per_burst=100)
        ilr.update(np.arange(100, dtype=float) / 100.0)
        assert np.allclose(ilr.profile, ilr.matrix.reshape(-1))

    def test_update_follows_learning_law_without_smoothing(self) -> None:
        config = ILRConfig(learning_rate=0.5, smoothing_depth=1)
        ilr = IterativeLearningController(config, initial_command=4.0, pulses_per_burst=100)
        errors = np.full(100, 2.0)

        ilr.update(errors)

        assert np.allclose(ilr.matrix, 5.0)  # 4.0 + 0.5 * 2.0

    def test_smoothing_reduces_noise_in_the_learned_profile(self) -> None:
        rng = np.random.default_rng(3)
        errors = rng.normal(0.0, 1.0, size=100)

        raw = IterativeLearningController(ILRConfig(learning_rate=1.0, smoothing_depth=1), 0.0, 100)
        filtered = IterativeLearningController(
            ILRConfig(learning_rate=1.0, smoothing_depth=5), 0.0, 100
        )
        raw.update(errors)
        filtered.update(errors)

        assert filtered.matrix.std() < raw.matrix.std()

    def test_command_index_maps_into_matrix(self) -> None:
        ilr = IterativeLearningController(ILRConfig(), initial_command=1.0, pulses_per_burst=100)
        ilr._matrix[3, 7] = 9.0  # noqa: SLF001 - white-box check of the mapping
        assert ilr.command(37) == pytest.approx(9.0)

    def test_wrong_error_length_raises(self) -> None:
        ilr = IterativeLearningController(ILRConfig(), initial_command=1.0, pulses_per_burst=100)
        with pytest.raises(ValueError):
            ilr.update(np.zeros(50))

    def test_reset_restores_initial_command(self) -> None:
        ilr = IterativeLearningController(ILRConfig(), initial_command=4.0, pulses_per_burst=100)
        ilr.update(np.full(100, 1.0))
        ilr.reset()
        assert np.allclose(ilr.matrix, 4.0)
        assert ilr.bursts_learned == 0

    def test_invalid_configuration_is_rejected(self) -> None:
        with pytest.raises(ValueError):
            IterativeLearningController(ILRConfig(learning_rate=0.0), 1.0, 100)
        with pytest.raises(ValueError):
            IterativeLearningController(ILRConfig(grid_shape=(3, 3)), 1.0, 100)


class TestHybrid:
    def test_feedforward_and_feedback_sum_to_command(self) -> None:
        hybrid = HybridController(
            learning=IterativeLearningController(
                ILRConfig(), initial_command=4.0, pulses_per_burst=100
            ),
            pid=PIDController(PIDConfig(kp=0.5, ki=0.0, kd=0.0)),
        )
        actuation = hybrid.act(pulse_index=5, target=4.0, measurement=3.0)
        assert actuation.feedforward == pytest.approx(4.0)
        assert actuation.feedback == pytest.approx(0.5)
        assert actuation.command == pytest.approx(4.5)

    def test_command_is_never_negative(self) -> None:
        hybrid = HybridController(
            learning=IterativeLearningController(
                ILRConfig(), initial_command=0.1, pulses_per_burst=100
            ),
            pid=PIDController(PIDConfig(kp=5.0, ki=0.0, kd=0.0)),
        )
        actuation = hybrid.act(pulse_index=0, target=4.0, measurement=10.0)
        assert actuation.command == 0.0
