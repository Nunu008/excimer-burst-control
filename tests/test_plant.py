"""Tests for the plant model and the efficiency profile."""

from __future__ import annotations

import numpy as np
import pytest
from src.core.config import PlantConfig
from src.core.plant import ExcimerPlant


def test_efficiency_recovers_after_startup_dip_then_decays() -> None:
    plant = ExcimerPlant(PlantConfig())

    assert plant.efficiency(0) < plant.efficiency(10)  # startup dip recovers
    assert plant.efficiency(99) < plant.efficiency(50)  # gas degrades over the burst
    assert 0.5 < plant.efficiency(99) < 0.75


def test_nominal_command_hits_target_on_first_pulse() -> None:
    config = PlantConfig()
    plant = ExcimerPlant(config)

    energy = plant.emit(config.nominal_command, 0)
    assert energy == pytest.approx(config.target_energy_mj)


def test_energy_scales_linearly_with_command() -> None:
    plant = ExcimerPlant(PlantConfig())

    low = plant.emit(4.0, 17)
    high = plant.emit(8.0, 17)
    assert high == pytest.approx(2.0 * low)


def test_energy_never_negative() -> None:
    plant = ExcimerPlant(PlantConfig())
    assert plant.emit(-3.0, 0) == 0.0


def test_noise_factors_are_clipped_and_centred() -> None:
    plant = ExcimerPlant(PlantConfig(noise_sigma_rel=0.05))
    rng = np.random.default_rng(7)

    factors = plant.noise_factors(rng, 20_000)

    assert factors.min() >= 0.2
    assert abs(factors.mean() - 1.0) < 0.01


def test_zero_noise_gives_unity_factors() -> None:
    plant = ExcimerPlant(PlantConfig(noise_sigma_rel=0.0))
    factors = plant.noise_factors(np.random.default_rng(1), 64)
    assert np.all(factors == 1.0)
