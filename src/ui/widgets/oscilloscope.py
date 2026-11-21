"""Main oscilloscope comparing the three control strategies."""

from __future__ import annotations

import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QVBoxLayout, QWidget

from ...core.simulation import MODE_COLORS, MODE_LABELS, BurstSnapshot
from ..theme import GRID, PANEL, TEXT_MUTED


class OscilloscopeWidget(QWidget):
    """Live pulse-energy trace with the ±tolerance band drawn behind the curves."""

    def __init__(self, tolerance_band: float = 0.02, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._tolerance_band = tolerance_band

        self._plot = pg.PlotWidget()
        self._plot.setBackground(PANEL)
        self._plot.showGrid(x=True, y=True, alpha=0.18)
        self._plot.setLabel("left", "Pulse energy", units="mJ")
        self._plot.setLabel("bottom", "Pulse index in burst")
        self._plot.setMouseEnabled(x=False, y=True)
        self._plot.addLegend(offset=(-10, 10), labelTextColor=TEXT_MUTED)
        self._plot.getAxis("left").setTextPen(TEXT_MUTED)
        self._plot.getAxis("bottom").setTextPen(TEXT_MUTED)
        self._plot.getAxis("left").setPen(pg.mkPen(GRID))
        self._plot.getAxis("bottom").setPen(pg.mkPen(GRID))

        self._band_upper = pg.PlotDataItem(pen=None)
        self._band_lower = pg.PlotDataItem(pen=None)
        self._plot.addItem(self._band_upper)
        self._plot.addItem(self._band_lower)
        self._band = pg.FillBetweenItem(
            self._band_upper,
            self._band_lower,
            brush=pg.mkBrush(79, 209, 197, 26),
        )
        self._plot.addItem(self._band)

        self._curves: dict[str, pg.PlotDataItem] = {}
        for mode, colour in MODE_COLORS.items():
            self._curves[mode] = self._plot.plot(
                [],
                [],
                pen=pg.mkPen(colour, width=1.7),
                name=MODE_LABELS[mode],
            )

        self._target_line = pg.InfiniteLine(
            angle=0,
            pen=pg.mkPen(TEXT_MUTED, width=1, style=Qt.PenStyle.DashLine),
            label="target",
            labelOpts={"color": TEXT_MUTED, "position": 0.04},
        )
        self._plot.addItem(self._target_line)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._plot)

    @property
    def plot(self) -> pg.PlotWidget:
        return self._plot

    def update_snapshot(self, snapshot: BurstSnapshot) -> None:
        target = snapshot.target_mj
        band = self._tolerance_band * target
        x = np.arange(len(snapshot.result(next(iter(self._curves))).energy_mj))

        for mode, curve in self._curves.items():
            curve.setData(x, snapshot.result(mode).energy_mj)

        self._band_upper.setData(x, np.full(x.size, target + band))
        self._band_lower.setData(x, np.full(x.size, target - band))
        self._target_line.setValue(target)
        self._plot.setYRange(target * 0.78, target * 1.22, padding=0)
