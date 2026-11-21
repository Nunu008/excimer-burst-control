"""Headless screenshot capture for the README (no physical display needed).

Usage:
    QT_QPA_PLATFORM=offscreen python scripts/capture_screenshots.py
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from PySide6.QtWidgets import QApplication  # noqa: E402
from src.ui.main_window import MainWindow  # noqa: E402
from src.ui.theme import STYLESHEET  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "screenshots"


def capture(window: MainWindow, name: str) -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / name
    window.grab().save(str(path))
    print(f"wrote {path}")


def main() -> int:
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setStyleSheet(STYLESHEET)

    window = MainWindow()
    window.resize(1560, 960)
    window.show()

    window._advance(14)  # noqa: SLF001 - same path the UI timer uses
    capture(window, "dashboard.png")

    window._on_preset("noise")  # noqa: SLF001
    window._advance(12)  # noqa: SLF001
    capture(window, "gas-noise.png")

    window._on_preset("latency")  # noqa: SLF001
    window._advance(10)  # noqa: SLF001
    capture(window, "latency.png")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
