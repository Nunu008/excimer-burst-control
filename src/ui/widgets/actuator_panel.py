"""Actuator breakdown: feedforward share versus PID share in the hybrid loop."""

from __future__ import annotations

import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QVBoxLayout, QWidget

from ...core.simulation import MODE_HYBRID, BurstSnapshot
from ..theme import ACCENT, DANGER, GRID, PANEL, TEXT_MUTED, WARNING


class ActuatorPanel(QWidget):
    """Shows how the learned table takes over while the PID contribution decays."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)

        self._plot = pg.PlotWidget()
        self._plot.setBackground(PANEL)
        self._plot.showGrid(x=True, y=True, alpha=0.18)
        self._plot.setLabel("left", "Actuator command", units="mJ-eq")
        self._plot.setLabel("bottom", "Pulse index in burst")
        self._plot.setMouseEnabled(x=False, y=False)
        self._plot.addLegend(offset=(-10, 10), labelTextColor=TEXT_MUTED)
        self._plot.getAxis("left").setTextPen(TEXT_MUTED)
        self._plot.getAxis("bottom").setTextPen(TEXT_MUTED)
        self._plot.getAxis("left").setPen(pg.mkPen(GRID))
        self._plot.getAxis("bottom").setPen(pg.mkPen(GRID))

        self._feedforward = self._plot.plot(
            [], [], pen=pg.mkPen(ACCENT, width=1.6), name="u_ff (ILR)"
        )
        self._feedback = self._plot.plot(
            [], [], pen=pg.mkPen(WARNING, width=1.4), name="u_fb (PID)"
        )
        self._command = self._plot.plot(
            [],
            [],
            pen=pg.mkPen(TEXT_MUTED, width=1, style=Qt.PenStyle.DashLine),
            name="u_total",
        )
        self._zero = pg.InfiniteLine(
            angle=0, pen=pg.mkPen(DANGER, width=1, style=Qt.PenStyle.DotLine)
        )
        self._plot.addItem(self._zero)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._plot)

    @property
    def plot(self) -> pg.PlotWidget:
        return self._plot

    def update_snapshot(self, snapshot: BurstSnapshot) -> None:
        result = snapshot.result(MODE_HYBRID)
        x = np.arange(result.energy_mj.size)
        self._feedforward.setData(x, result.feedforward)
        self._feedback.setData(x, result.feedback)
        self._command.setData(x, result.command)
        self._plot.setYRange(-1.5, 9.0, padding=0)
