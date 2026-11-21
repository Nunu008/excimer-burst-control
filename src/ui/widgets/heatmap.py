"""10x10 heat map of the learned feedforward matrix with hover read-out."""

from __future__ import annotations

import numpy as np
import pyqtgraph as pg
from PySide6.QtCore import QPointF, Qt
from PySide6.QtWidgets import QLabel, QVBoxLayout, QWidget

from ..theme import GRID, PANEL, TEXT_MUTED


class HeatmapWidget(QWidget):
    """Displays the ILR matrix; every cell is one pulse of the burst."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._matrix = np.zeros((10, 10), dtype=float)
        self._target = 4.0

        self._plot = pg.PlotWidget()
        self._plot.setBackground(PANEL)
        self._plot.setMouseEnabled(x=False, y=False)
        self._plot.setLabel("left", "Block (pulse // 10)")
        self._plot.setLabel("bottom", "Pulse within block")
        self._plot.invertY(True)
        self._plot.getAxis("left").setTextPen(TEXT_MUTED)
        self._plot.getAxis("bottom").setTextPen(TEXT_MUTED)
        self._plot.getAxis("left").setPen(pg.mkPen(GRID))
        self._plot.getAxis("bottom").setPen(pg.mkPen(GRID))
        self._plot.getAxis("left").setTicks([[(i + 0.5, str(i)) for i in range(10)]])
        self._plot.getAxis("bottom").setTicks([[(i + 0.5, str(i)) for i in range(10)]])

        self._image = pg.ImageItem(axisOrder="row-major")
        self._image.setColorMap(pg.colormap.get("inferno"))
        self._plot.addItem(self._image)

        self._info = QLabel("Hover a cell to inspect the learned command.")
        self._info.setObjectName("hint")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._plot, 1)
        layout.addWidget(self._info)

        self._plot.scene().sigMouseMoved.connect(self._on_mouse_moved)

    @property
    def plot(self) -> pg.PlotWidget:
        return self._plot

    def update_matrix(self, matrix: np.ndarray, target: float) -> None:
        self._matrix = np.asarray(matrix, dtype=float)
        self._target = target
        low, high = target * 0.5, target * 1.9
        self._image.setImage(self._matrix, autoLevels=False, levels=(low, high))
        self._image.setRect(0.0, 0.0, self._matrix.shape[1], self._matrix.shape[0])

    def _on_mouse_moved(self, position: QPointF) -> None:
        if not self._plot.sceneBoundingRect().contains(position):
            return
        point = self._plot.getViewBox().mapSceneToView(position)
        column = int(np.floor(point.x()))
        row = int(np.floor(point.y()))
        rows, columns = self._matrix.shape
        if not (0 <= row < rows and 0 <= column < columns):
            return
        pulse_index = row * columns + column
        value = self._matrix[row, column]
        deviation = (value - self._target) / self._target * 100.0
        self._info.setText(
            f"Pulse {pulse_index:3d}  ·  u_ff = {value:6.3f} mJ-eq  ·  {deviation:+.1f} % vs target"
        )


__all__ = ["HeatmapWidget", "Qt"]
