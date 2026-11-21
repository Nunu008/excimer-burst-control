"""Tests for the moving-average filter."""

from __future__ import annotations

import numpy as np
import pytest
from src.core.filters import moving_average


def test_depth_one_returns_copy() -> None:
    values = np.array([1.0, 2.0, 3.0])
    result = moving_average(values, 1)
    assert np.array_equal(result, values)
    assert result is not values


def test_constant_signal_is_unchanged() -> None:
    result = moving_average(np.full(50, 3.5), 7)
    assert np.allclose(result, 3.5)


def test_output_length_is_preserved() -> None:
    assert moving_average(np.arange(100.0), 3).size == 100
    assert moving_average(np.arange(100.0), 9).size == 100


def test_smoothing_reduces_variance() -> None:
    rng = np.random.default_rng(11)
    noise = rng.normal(size=500)
    smoothed = moving_average(noise, 9)
    assert smoothed.std() < noise.std()


def test_centred_window_preserves_step_location() -> None:
    step = np.zeros(101)
    step[50:] = 1.0
    smoothed = moving_average(step, 5)
    assert abs(float(np.argmax(smoothed > 0.5)) - 50) <= 2


def test_rejects_two_dimensional_input() -> None:
    with pytest.raises(ValueError):
        moving_average(np.zeros((3, 3)), 3)
