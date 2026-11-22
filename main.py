"""Entry point of the excimer burst-energy control dashboard.

python main.py               start the dashboard
python main.py --self-test [--report FILE]
                             build the UI, run 60 bursts, check the control claims,
                             exit 0/1 (used by CI on the packaged executables)
python main.py --version
"""

from __future__ import annotations

import argparse
import sys

__version__ = "2.0.0"

SELF_TEST_BURSTS = 60


def _self_test(window: object, report: str | None) -> int:
    """Drive the real window like the UI timer does and verify the result."""
    from src.core.simulation import MODE_HYBRID, MODE_PID

    window.advance(SELF_TEST_BURSTS)  # type: ignore[attr-defined]
    snapshot = window.snapshot  # type: ignore[attr-defined]
    hybrid = snapshot.result(MODE_HYBRID).kpis
    pid = snapshot.result(MODE_PID).kpis
    rendered = not window.grab().isNull()  # type: ignore[attr-defined]

    converged = hybrid.max_deviation_pct < 5.0
    beats_pid = hybrid.rms_mj < 0.5 * pid.rms_mj
    checks = {
        f"hybrid max deviation {hybrid.max_deviation_pct:.2f} % < 5 %": converged,
        f"hybrid RMS {hybrid.rms_mj:.3f} mJ < 0.5 x PID RMS {pid.rms_mj:.3f} mJ": beats_pid,
        "dashboard renders": rendered,
    }
    lines = [f"Excimer Burst Control {__version__} self-test, burst {snapshot.index}"]
    lines += [f"  [{'PASS' if ok else 'FAIL'}] {text}" for text, ok in checks.items()]
    text = "\n".join(lines) + "\n"
    print(text, end="")  # no-op in a windowed executable, which has no stdout
    if report:
        with open(report, "w", encoding="utf-8") as handle:
            handle.write(text)
    return 0 if all(checks.values()) else 1


def main(argv: list[str] | None = None) -> int:
    """Start the Qt application and show the dashboard."""
    parser = argparse.ArgumentParser(
        prog="ExcimerBurstControl", description=__doc__.splitlines()[0]
    )
    parser.add_argument("--self-test", action="store_true", help="run the built-in check and exit")
    parser.add_argument("--report", metavar="FILE", help="also write the self-test result here")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    args, qt_args = parser.parse_known_args(argv)

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

    app = QApplication([sys.argv[0], *qt_args])
    app.setApplicationName("Excimer Burst Control")
    app.setApplicationVersion(__version__)
    app.setOrganizationName("Laser Control Lab")
    app.setStyle("Fusion")
    app.setStyleSheet(STYLESHEET)

    window = MainWindow()
    window.show()
    if args.self_test:
        return _self_test(window, args.report)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
