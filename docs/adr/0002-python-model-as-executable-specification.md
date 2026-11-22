# 2. The Python model is the executable specification

* Status: Accepted
* Date: 2026-11-22

## Context

The control law (ILC with Q-filter, PID with anti-windup, Timelog dead time, KPI
definitions) was developed and tuned in Python, where it is easy to visualise and
test. The next step is firmware that runs the same law on bare-metal Arm cores. Two
implementations of one algorithm drift apart unless something forces them together.

## Decision

`src/core` stays the reference. Any embedded implementation must reproduce it:

* identical parameter defaults and presets,
* identical update order within a pulse and within a burst,
* identical KPI definitions,

and this is checked by a test that replays golden vectors exported from Python
(`scripts/export_golden.py`) through the C++ code. A change to the control law is a
change to the Python model first, then to the firmware, in the same pull request.

## Consequences

* Tuning and visualisation stay in the fast Python loop; the firmware inherits proven
  behaviour instead of being tuned on hardware.
* Deliberate differences on the target (single precision, a constant-time noise
  generator, double-buffered learning) must be explained and bounded by a test.
* The golden vectors are committed and reviewed. A diff in them is a visible
  signal that the behaviour changed.
