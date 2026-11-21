"""Entry point of the excimer burst-energy control dashboard."""

from __future__ import annotations

import sys


def main() -> int:
    """Start the Qt application and show the dashboard."""
    try:
        from PySide6.QtWidgets import QApplication
    except ImportError as exc:  # pragma: no cover - only hit without Qt installed
        print(
            "PySide6 is not installed. Install the runtime dependencies first:\n"
            "    pip install -r requirements.txt\n"
            f"Original error: {exc}",
            file=sys.stderr,
        )
        return 1

    from src.ui.main_window import MainWindow
    from src.ui.theme import STYLESHEET

    app = QApplication(sys.argv)
    app.setApplicationName("Excimer Burst Control")
    app.setOrganizationName("Laser Control Lab")
    app.setStyle("Fusion")
    app.setStyleSheet(STYLESHEET)

    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
