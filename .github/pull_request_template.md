## What and why

<!-- One or two sentences. Link the issue this closes. -->

Closes #

## How it was verified

<!-- Commands you ran and what they showed. "Tests pass" is not enough: say which. -->

- [ ] `python -m pytest -q` (Python reference model)
- [ ] `ctest --preset host` (firmware host build)
- [ ] Cross builds for the affected targets (`cmake --workflow --preset <target>`)
- [ ] QEMU run on the affected targets, if the change touches a BSP or the control loop

## Control-engineering impact

<!-- Does this change the numbers in README section 4? If yes, update them in this PR
     and say why the new numbers are right. If the Python/C++ parity golden vectors
     changed, explain the model change that caused it. -->

- [ ] No change to the control law or its parameters
- [ ] Control law changed, README numbers and golden vectors regenerated

## Checklist

- [ ] Conventional Commit messages (`feat:`, `fix:`, `test:`, `docs:`, `ci:`, `chore:`)
- [ ] CHANGELOG.md `Unreleased` section updated
- [ ] An ADR in `docs/adr/` for any decision a reviewer would otherwise ask about
