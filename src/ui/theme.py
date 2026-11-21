"""Industrial dark theme (oscilloscope / lithography-control look)."""

from __future__ import annotations

BACKGROUND = "#0b0f14"
PANEL = "#121821"
PANEL_ALT = "#161e29"
BORDER = "#202b3a"
TEXT = "#d7e0ea"
TEXT_MUTED = "#7d8b9c"
ACCENT = "#4fd1c5"
WARNING = "#e0a33e"
DANGER = "#d95f5f"
GRID = "#1b2634"

STYLESHEET = f"""
QWidget {{
    background-color: {BACKGROUND};
    color: {TEXT};
    font-family: "Inter", "Segoe UI", "DejaVu Sans", sans-serif;
    font-size: 12px;
}}
QMainWindow, QDialog {{
    background-color: {BACKGROUND};
}}
QFrame#card {{
    background-color: {PANEL};
    border: 1px solid {BORDER};
    border-radius: 8px;
}}
QFrame#sidebar {{
    background-color: {PANEL_ALT};
    border-left: 1px solid {BORDER};
}}
QLabel#cardTitle {{
    color: {TEXT_MUTED};
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 1px;
}}
QLabel#cardValue {{
    color: {TEXT};
    font-size: 19px;
    font-weight: 600;
}}
QLabel#cardUnit {{
    color: {TEXT_MUTED};
    font-size: 10px;
}}
QLabel#sectionTitle {{
    color: {ACCENT};
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 1.5px;
    padding-top: 6px;
}}
QLabel#hint {{
    color: {TEXT_MUTED};
    font-size: 10px;
}}
QPushButton {{
    background-color: {PANEL};
    border: 1px solid {BORDER};
    border-radius: 5px;
    padding: 6px 10px;
    color: {TEXT};
}}
QPushButton:hover {{
    border-color: {ACCENT};
}}
QPushButton:checked {{
    background-color: {ACCENT};
    color: #06282a;
    font-weight: 700;
}}
QSlider::groove:horizontal {{
    height: 3px;
    background: {BORDER};
    border-radius: 2px;
}}
QSlider::handle:horizontal {{
    background: {ACCENT};
    width: 12px;
    margin: -5px 0;
    border-radius: 6px;
}}
QSlider::sub-page:horizontal {{
    background: {ACCENT};
    border-radius: 2px;
}}
QComboBox {{
    background-color: {PANEL};
    border: 1px solid {BORDER};
    border-radius: 5px;
    padding: 4px 8px;
}}
QScrollBar:vertical {{
    background: {BACKGROUND};
    width: 8px;
}}
QScrollBar::handle:vertical {{
    background: {BORDER};
    border-radius: 4px;
    min-height: 24px;
}}
QToolTip {{
    background-color: {PANEL_ALT};
    color: {TEXT};
    border: 1px solid {ACCENT};
    padding: 4px;
}}
"""
