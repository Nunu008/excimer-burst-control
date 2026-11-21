"""Sidebar with transport controls, presets and live parameter sliders."""

from __future__ import annotations

from collections.abc import Iterable

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)

from ...core.config import PRESETS


class ParameterSlider(QWidget):
    """Labelled slider with a live numeric read-out."""

    valueChanged = Signal(float)

    def __init__(
        self,
        label: str,
        minimum: float,
        maximum: float,
        value: float,
        decimals: int = 2,
        suffix: str = "",
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self._scale = 10**decimals
        self._decimals = decimals
        self._suffix = suffix

        self._name = QLabel(label)
        self._value = QLabel()
        self._value.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        self._value.setStyleSheet("color: #4fd1c5; font-weight: 600;")

        header = QHBoxLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.addWidget(self._name)
        header.addStretch(1)
        header.addWidget(self._value)

        self._slider = QSlider(Qt.Orientation.Horizontal)
        self._slider.setRange(int(round(minimum * self._scale)), int(round(maximum * self._scale)))
        self._slider.setValue(int(round(value * self._scale)))
        self._slider.valueChanged.connect(self._on_slider_changed)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 2, 0, 2)
        layout.setSpacing(2)
        layout.addLayout(header)
        layout.addWidget(self._slider)

        self._update_label(float(self._slider.value()) / self._scale)

    def value(self) -> float:
        return self._slider.value() / self._scale

    def set_value(self, value: float) -> None:
        self._slider.blockSignals(True)
        self._slider.setValue(int(round(value * self._scale)))
        self._slider.blockSignals(False)
        self._update_label(self.value())

    def _on_slider_changed(self, raw: int) -> None:
        value = raw / self._scale
        self._update_label(value)
        self.valueChanged.emit(value)

    def _update_label(self, value: float) -> None:
        self._value.setText(f"{value:.{self._decimals}f}{self._suffix}")


PRESET_BUTTONS: dict[str, tuple[str, str]] = {
    "standard": ("2 kHz", PRESETS["standard"]),
    "noise": ("Noise", PRESETS["noise"]),
    "latency": ("Latency", PRESETS["latency"]),
}


class ControlPanel(QFrame):
    """All operator interactions of the dashboard."""

    playToggled = Signal(bool)
    stepRequested = Signal(int)
    resetRequested = Signal()
    presetRequested = Signal(str)
    parametersChanged = Signal(dict)
    exportRequested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("sidebar")
        self.setFixedWidth(308)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(8)

        layout.addWidget(self._section("TRANSPORT"))
        transport = QHBoxLayout()
        transport.setSpacing(6)
        self._play_button = QPushButton("▶  Play")
        self._play_button.setCheckable(True)
        self._play_button.toggled.connect(self._on_play_toggled)
        step_one = QPushButton("+1")
        step_one.setToolTip("Advance one burst")
        step_one.clicked.connect(lambda: self.stepRequested.emit(1))
        step_five = QPushButton("+5")
        step_five.setToolTip("Advance five bursts")
        step_five.clicked.connect(lambda: self.stepRequested.emit(5))
        reset = QPushButton("Reset")
        reset.clicked.connect(self.resetRequested.emit)
        for widget in (self._play_button, step_one, step_five, reset):
            transport.addWidget(widget)
        layout.addLayout(transport)

        layout.addWidget(self._section("PRESETS"))
        presets = QHBoxLayout()
        presets.setSpacing(6)
        for key, (short, full) in PRESET_BUTTONS.items():
            button = QPushButton(short)
            button.setToolTip(full)
            button.clicked.connect(lambda _checked=False, key=key: self.presetRequested.emit(key))
            presets.addWidget(button)
        layout.addLayout(presets)

        self._sliders: dict[str, ParameterSlider] = {}
        self._add_group(
            layout,
            "PROCESS",
            {
                "target_mj": ParameterSlider("Target energy", 2.0, 8.0, 4.0, 1, " mJ"),
                "decay_rate": ParameterSlider("Gas decay rate", 0.0, 0.02, 0.004, 3, " /pulse"),
                "noise_sigma": ParameterSlider("Pulse-to-pulse noise", 0.0, 0.10, 0.01, 3, ""),
                "delay": ParameterSlider("Timelog latency", 1, 3, 2, 0, " pulses"),
            },
        )
        self._add_group(
            layout,
            "PID",
            {
                "kp": ParameterSlider("Kp", 0.0, 1.5, 0.55, 2, ""),
                "ki": ParameterSlider("Ki", 0.0, 0.30, 0.06, 3, ""),
                "kd": ParameterSlider("Kd", 0.0, 0.20, 0.02, 3, ""),
            },
        )
        self._add_group(
            layout,
            "ITERATIVE LEARNING (ILR)",
            {
                "alpha": ParameterSlider("Learning factor α", 0.05, 1.0, 0.6, 2, ""),
                "smoothing": ParameterSlider("Q-filter depth", 1, 9, 3, 0, " pulses"),
            },
        )

        layout.addStretch(1)
        export = QPushButton("⤓  Export current burst as CSV")
        export.clicked.connect(self.exportRequested.emit)
        layout.addWidget(export)

        hint = QLabel("Space: play/pause · →: +1 burst")
        hint.setObjectName("hint")
        hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(hint)

    # ------------------------------------------------------------------ values

    def values(self) -> dict[str, float]:
        return {name: slider.value() for name, slider in self._sliders.items()}

    def set_playing(self, playing: bool) -> None:
        self._play_button.blockSignals(True)
        self._play_button.setChecked(playing)
        self._play_button.setText("❚❚  Pause" if playing else "▶  Play")
        self._play_button.blockSignals(False)

    def apply_values(self, values: dict[str, float]) -> None:
        """Push configuration values into the sliders without emitting changes."""
        for name, value in values.items():
            slider = self._sliders.get(name)
            if slider is not None:
                slider.blockSignals(True)
                slider.set_value(value)
                slider.blockSignals(False)

    # ----------------------------------------------------------------- signals

    def _emit_parameters(self, _value: float | None = None) -> None:
        self.parametersChanged.emit(self.values())

    def _on_play_toggled(self, playing: bool) -> None:
        self._play_button.setText("❚❚  Pause" if playing else "▶  Play")
        self.playToggled.emit(playing)

    def _add_group(
        self, layout: QVBoxLayout, title: str, sliders: dict[str, ParameterSlider]
    ) -> None:
        layout.addWidget(self._section(title))
        for name, slider in sliders.items():
            slider.valueChanged.connect(self._emit_parameters)
            self._sliders[name] = slider
            layout.addWidget(slider)

    @staticmethod
    def _section(title: str) -> QLabel:
        label = QLabel(title)
        label.setObjectName("sectionTitle")
        return label


def iter_sliders(panel: ControlPanel) -> Iterable[ParameterSlider]:
    """Iterate over the panel sliders (handy for tests)."""
    return panel._sliders.values()
