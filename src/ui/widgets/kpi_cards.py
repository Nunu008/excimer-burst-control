"""KPI cards comparing the three control strategies at a glance."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QFrame, QGridLayout, QHBoxLayout, QLabel, QVBoxLayout, QWidget

from ...core.metrics import KPIs
from ..theme import ACCENT, DANGER, TEXT_MUTED, WARNING


class KPICard(QFrame):
    """Compact card showing the four headline KPIs of one controller."""

    def __init__(self, title: str, accent: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("card")
        self._accent = accent

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(6)

        header = QHBoxLayout()
        header.setSpacing(6)
        dot = QLabel("●")
        dot.setStyleSheet(f"color: {accent}; font-size: 11px;")
        title_label = QLabel(title.upper())
        title_label.setObjectName("cardTitle")
        header.addWidget(dot)
        header.addWidget(title_label)
        header.addStretch(1)
        layout.addLayout(header)

        values = QGridLayout()
        values.setHorizontalSpacing(10)
        values.setVerticalSpacing(2)

        self._values: dict[str, QLabel] = {}
        specs = [
            ("max", "Max dev", "%"),
            ("std", "σ", "mJ"),
            ("rms", "RMS", "mJ"),
            ("settle", "Settling", "pulse"),
        ]
        for column, (key, label, unit) in enumerate(specs):
            caption = QLabel(f"{label} [{unit}]")
            caption.setObjectName("cardUnit")
            value = QLabel("—")
            value.setObjectName("cardValue")
            value.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
            values.addWidget(caption, 0, column)
            values.addWidget(value, 1, column)
            self._values[key] = value
        layout.addLayout(values)

    def update_kpis(self, kpis: KPIs) -> None:
        self._values["max"].setText(f"{kpis.max_deviation_pct:.2f}")
        self._values["std"].setText(f"{kpis.std_mj:.3f}")
        self._values["rms"].setText(f"{kpis.rms_mj:.3f}")
        self._values["settle"].setText(
            f"{kpis.settling_pulse}" if kpis.settling_pulse is not None else "—"
        )
        colour = TEXT_MUTED
        if kpis.max_deviation_pct <= 2.0:
            colour = ACCENT
        elif kpis.max_deviation_pct <= 10.0:
            colour = WARNING
        else:
            colour = DANGER
        self._values["max"].setStyleSheet(f"color: {colour};")


class KPIBar(QWidget):
    """Horizontal arrangement of the three controller cards."""

    def __init__(
        self, titles: dict[str, str], colors: dict[str, str], parent: QWidget | None = None
    ) -> None:
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(10)
        self._cards = {mode: KPICard(title, colors[mode]) for mode, title in titles.items()}
        for card in self._cards.values():
            layout.addWidget(card, 1)

    def update_kpis(self, kpis_by_mode: dict[str, KPIs]) -> None:
        for mode, can in self._cards.items():
            if mode in kpis_by_mode:
                can.update_kpis(kpis_by_mode[mode])
