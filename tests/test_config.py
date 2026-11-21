"""Tests for configuration validation and presets."""

from __future__ import annotations

import pytest
from src.core.config import ILRConfig, PlantConfig, SimulationConfig, apply_preset


def test_default_configuration_is_consistent() -> None:
    config = SimulationConfig()
    assert config.pulses_per_burst == 100
    assert config.ilr.grid_shape == (10, 10)
    assert config.measurement_delay_pulses == 2


def test_efficiency_profile_shape() -> None:
    plant = PlantConfig()
    assert plant.efficiency(0) == pytest.approx(0.85)
    assert 0.6 < plant.efficiency(99) < 0.7


def test_negative_delay_is_rejected() -> None:
    with pytest.raises(ValueError):
        SimulationConfig(measurement_delay_pulses=0)


def test_invalid_tolerance_band_is_rejected() -> None:
    with pytest.raises(ValueError):
        SimulationConfig(tolerance_band=0.7)


def test_grid_must_cover_the_burst() -> None:
    with pytest.raises(ValueError):
        SimulationConfig(ilr=ILRConfig(grid_shape=(8, 8)))


def test_presets_are_distinct_and_valid() -> None:
    standard = apply_preset("standard")
    noisy = apply_preset("noise")
    latency = apply_preset("latency")

    assert noisy.plant.noise_sigma_rel > standard.plant.noise_sigma_rel
    assert latency.measurement_delay_pulses == 3
    assert latency.pid == standard.pid  # only the dead time changes


def test_unknown_preset_raises() -> None:
    with pytest.raises(ValueError):
        apply_preset("does-not-exist")
