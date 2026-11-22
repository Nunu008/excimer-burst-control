"""The committed golden vectors must match the current Python model (ADR 0002).

If this test fails, the control law changed: regenerate the vectors with
``python scripts/export_golden.py`` and let the C++ parity test confirm that the
firmware follows.
"""

from __future__ import annotations

from scripts.export_golden import main


def test_golden_vectors_are_up_to_date() -> None:
    assert main(["--check"]) == 0
