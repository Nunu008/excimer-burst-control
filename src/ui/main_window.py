"""Main window wiring the simulation engine to the dashboard widgets."""

from __future__ import annotations

import csv
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import (
    QFileDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QVBoxLayout,
    QWidget,
)

from ..core.config import PRESETS, SimulationConfig, apply_preset
from ..core.simulation import (
    MODE_COLORS,
    MODE_HYBRID,
    MODE_LABELS,
    MODE_OPEN_LOOP,
    MODE_PID,
    SimulationEngine,
)
from .theme import TEXT_MUTED
from .widgets.actuator_panel import ActuatorPanel
from .widgets.control_panel import ControlPanel
from .widgets.heatmap import HeatmapWidget
from .widgets.kpi_cards import KPIBar
from .widgets.oscilloscope import OscilloscopeWidget

REFRESH_INTERVAL_MS = 40


class MainWindow(QMainWindow):
    """Industrial dashboard: KPIs, oscilloscope, actuator split and ILR matrix."""

    def __init__(self, config: SimulationConfig | None = None) -> None:
        super().__init__()
        self.setWindowTitle("Excimer Burst Energy Control — 2 kHz ILR + PID")
        self.resize(1560, 960)

        self._config = config or SimulationConfig()
        self._engine = SimulationEngine(self._config)
        self._snapshot = self._engine.step()
        self._previous_matrix = self._snapshot.matrix.copy()
        self._playing = False

        self._timer = QTimer(self)
        self._timer.setInterval(REFRESH_INTERVAL_MS)
        self._timer.timeout.connect(lambda: self._advance(1))

        self._build_ui()
        self._install_shortcuts()
        self._refresh()

    # -------------------------------------------------------------------- UI

    def _build_ui(self) -> None:
        central = QWidget()
        root = QHBoxLayout(central)
        root.setContentsMargins(12, 12, 0, 12)
        root.setSpacing(12)

        column = QVBoxLayout()
        column.setSpacing(10)

        self._kpi_bar = KPIBar(MODE_LABELS, MODE_COLORS)
        column.addWidget(self._kpi_bar)

        self._scope = OscilloscopeWidget(self._config.tolerance_band)
        column.addWidget(self._wrap(self._scope, "PULSE ENERGY — THREE STRATEGIES"), 3)

        bottom = QHBoxLayout()
        bottom.setSpacing(10)
        self._actuators = ActuatorPanel()
        self._heatmap = HeatmapWidget()
        bottom.addWidget(self._wrap(self._actuators, "ACTUATOR BREAKDOWN — HYBRID"), 1)
        bottom.addWidget(self._wrap(self._heatmap, "LEARNED FEEDFORWARD MATRIX (10 × 10)"), 1)
        column.addLayout(bottom, 2)

        root.addLayout(column, 1)

        self._panel = ControlPanel()
        self._panel.playToggled.connect(self._on_play_toggled)
        self._panel.stepRequested.connect(self._advance)
        self._panel.resetRequested.connect(self._reset)
        self._panel.presetRequested.connect(self._on_preset)
        self._panel.parametersChanged.connect(self._on_parameters_changed)
        self._panel.exportRequested.connect(self._export)
        root.addWidget(self._panel)

        self.setCentralWidget(central)

        self._status = QLabel()
        self._status.setStyleSheet(f"color: {TEXT_MUTED}; padding: 2px 6px;")
        self.statusBar().addPermanentWidget(self._status)

    def _wrap(self, widget: QWidget, title: str) -> QFrame:
        frame = QFrame()
        frame.setObjectName("card")
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(10, 8, 10, 8)
        layout.setSpacing(4)
        label = QLabel(title)
        label.setObjectName("cardTitle")
        layout.addWidget(label)
        layout.addWidget(widget, 1)
        return frame

    def _install_shortcuts(self) -> None:
        play = QShortcut(QKeySequence("Space"), self)
        play.activated.connect(self._toggle_play)
        step = QShortcut(QKeySequence("Right"), self)
        step.activated.connect(lambda: self._advance(1))
        reset = QShortcut(QKeySequence("Ctrl+R"), self)
        reset.activated.connect(self._reset)

    # ---------------------------------------------------------------- actions

    def _on_play_toggled(self, playing: bool) -> None:
        self._playing = playing
        self._panel.set_playing(playing)
        if playing:
            self._timer.start()
        else:
            self._timer.stop()

    def _toggle_play(self) -> None:
        self._on_play_toggled(not self._playing)

    def _advance(self, bursts: int) -> None:
        if bursts <= 0:
            return
        for _ in range(bursts):
            self._snapshot = self._engine.step()
        self._refresh()

    def _reset(self) -> None:
        self._engine.reset()
        self._snapshot = self._engine.step()
        self._previous_matrix = self._snapshot.matrix.copy()
        self._refresh()

    def _on_preset(self, key: str) -> None:
        config = apply_preset(key)
        self._panel.apply_values(self._slider_values(config))
        self._rebuild(config)

    def _on_parameters_changed(self, values: dict) -> None:
        config = replace(
            self._config,
            plant=replace(
                self._config.plant,
                target_energy_mj=float(values["target_mj"]),
                decay_rate=float(values["decay_rate"]),
                noise_sigma_rel=float(values["noise_sigma"]),
            ),
            pid=replace(
                self._config.pid,
                kp=float(values["kp"]),
                ki=float(values["ki"]),
                kd=float(values["kd"]),
            ),
            ilr=replace(
                self._config.ilr,
                learning_rate=float(values["alpha"]),
                smoothing_depth=int(values["smoothing"]),
            ),
            measurement_delay_pulses=int(values["delay"]),
        )
        self._rebuild(config)

    def _rebuild(self, config: SimulationConfig) -> None:
        self._config = config
        self._engine = SimulationEngine(config)
        self._snapshot = self._engine.step()
        self._previous_matrix = self._snapshot.matrix.copy()
        self._refresh()

    # ---------------------------------------------------------------- refresh

    def _refresh(self) -> None:
        snapshot = self._snapshot
        self._kpi_bar.update_kpis({mode: result.kpis for mode, result in snapshot.results.items()})
        self._scope.update_snapshot(snapshot)
        self._actuators.update_snapshot(snapshot)
        self._heatmap.update_matrix(snapshot.matrix, snapshot.target_mj)

        delta = float(np.max(np.abs(snapshot.matrix - self._previous_matrix)))
        self._previous_matrix = snapshot.matrix.copy()
        self._status.setText(
            f"burst {snapshot.index:03d}   ·   learned bursts {snapshot.learned_bursts:03d}"
            f"   ·   max |Δu_ff| {delta:.4f} mJ-eq"
        )

    # ----------------------------------------------------------------- export

    def _export(self) -> None:
        directory = QFileDialog.getExistingDirectory(self, "Select folder for the CSV export")
        if not directory:
            return
        snapshot = self._snapshot
        base = Path(directory)
        stamp = time.strftime("%Y%m%d-%H%M%S")

        pulses_path = base / f"burst_{snapshot.index:04d}_{stamp}_pulses.csv"
        open_loop = snapshot.result(MODE_OPEN_LOOP)
        pid_only = snapshot.result(MODE_PID)
        hybrid = snapshot.result(MODE_HYBRID)
        with pulses_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(
                [
                    "pulse_index",
                    "open_loop_mj",
                    "pid_only_mj",
                    "hybrid_mj",
                    "hybrid_command",
                    "hybrid_feedforward",
                    "hybrid_feedback",
                ]
            )
            for i in range(hybrid.energy_mj.size):
                writer.writerow(
                    [
                        i,
                        f"{open_loop.energy_mj[i]:.6f}",
                        f"{pid_only.energy_mj[i]:.6f}",
                        f"{hybrid.energy_mj[i]:.6f}",
                        f"{hybrid.command[i]:.6f}",
                        f"{hybrid.feedforward[i]:.6f}",
                        f"{hybrid.feedback[i]:.6f}",
                    ]
                )

        matrix_path = base / f"feedforward_matrix_{snapshot.index:04d}_{stamp}.csv"
        with matrix_path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["block"] + [f"col_{c:02d}" for c in range(snapshot.matrix.shape[1])])
            for row, values in enumerate(snapshot.matrix):
                writer.writerow([f"row_{row:02d}"] + [f"{value:.6f}" for value in values])

        QMessageBox.information(
            self,
            "Export complete",
            f"Wrote two files into\n{directory}\n\n{pulses_path.name}\n{matrix_path.name}",
        )

    # ----------------------------------------------------------------- helpers

    @staticmethod
    def _slider_values(config: SimulationConfig) -> dict[str, float]:
        return {
            "target_mj": config.plant.target_energy_mj,
            "decay_rate": config.plant.decay_rate,
            "noise_sigma": config.plant.noise_sigma_rel,
            "delay": config.measurement_delay_pulses,
            "kp": config.pid.kp,
            "ki": config.pid.ki,
            "kd": config.pid.kd,
            "alpha": config.ilr.learning_rate,
            "smoothing": config.ilr.smoothing_depth,
        }


__all__ = ["MainWindow", "PRESETS", "Qt"]
