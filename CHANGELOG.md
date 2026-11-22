# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Project governance: issue and PR templates, CODEOWNERS, label taxonomy,
  Dependabot, contribution rules and architecture decision records.
- `firmware/core`: header-only C++23 port of the control law (PID, ILC with
  Q-filter, Timelog dead time, plant model, KPIs) templated on the scalar type,
  without heap, exceptions or RTTI.
- Interrupt-safe `RealtimeController` with a double-buffered learning table and a
  wait-free hand-over between the pulse interrupt and the learning task.
- GoogleTest suite (double and float) porting every claim of the Python suite,
  plus compile-time `static_assert` tests; host simulator `ebc_host_sim`.
- Firmware CI: C++23, C++26 and ASan/UBSan builds.
- Cross-language parity: `scripts/export_golden.py` exports per-pulse golden
  vectors from the Python model; the C++ core reproduces them to 1e-9 (double)
  and 5e-4 (float) relative for all presets. A Python test fails when the
  committed vectors are stale.

## [1.0.0] - 2026-11-21

### Added
- Python reference model of a 2 kHz excimer burst: gas-state plant, Timelog dead
  time, PID, Iterative Learning Control with Q-filter, hybrid ILC + PID.
- PySide6 dashboard: three-strategy oscilloscope, KPI cards, actuator split,
  10 x 10 learning-table heat map, presets and CSV export.
- Test suite proving the control claims numerically; CI on Python 3.11 and 3.12.
- Headless screenshot capture for the documentation.

[Unreleased]: https://github.com/Nunu008/excimer-burst-control/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Nunu008/excimer-burst-control/releases/tag/v1.0.0
