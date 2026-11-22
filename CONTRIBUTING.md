# Contributing

This repository is run like a small product team's repository, even though it has
one maintainer. The rules below are what CI and review enforce.

## Workflow

1. Every change starts as an **issue** with acceptance criteria (templates in
   `.github/ISSUE_TEMPLATE`). Issues are grouped into **milestones**, one per release.
2. Work happens on a short-lived branch named `<type>/<topic>`, for example
   `feat/bsp-zynqmp-r5f` or `test/python-cpp-parity`.
3. A **pull request** closes the issue (`Closes #12`). It is merged only when CI is
   green; the PR template asks for the verification that was actually run.
4. `main` is always releasable. Releases are tagged `vMAJOR.MINOR.PATCH` and
   summarised in `CHANGELOG.md`.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/): `feat`, `fix`, `test`,
`docs`, `ci`, `build`, `refactor`, `chore`, with an optional scope such as
`feat(bsp-r5f): ...`. The body explains *why*; the diff already shows *what*.

## Definition of done

- Tests for the new behaviour, including the failure case.
- Python and C++ agree: if the control law changes, regenerate the golden vectors
  with `python scripts/export_golden.py` in the same PR.
- No new compiler warnings. Firmware builds with `-Werror` on all targets.
- Numbers quoted in the README come from a test or a CI log, never from memory.
- Architecture decisions that a reviewer would question are recorded as an ADR in
  `docs/adr/`.

## Local checks

```bash
# Python reference model
python -m pytest -q
python -m ruff check src tests main.py scripts

# Firmware, inside the dev container (or any machine with GCC >= 14)
cmake --workflow --preset host
```
