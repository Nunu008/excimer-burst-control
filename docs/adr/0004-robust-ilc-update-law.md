# 4. Apply the Q-filter to the whole learning update

* Status: Accepted (supersedes the update law of version 1.0)
* Date: 2026-11-22

## Context

Version 1.0 learned with `u_ff,k+1 = u_ff,k + α·Q(e_k)`: only the error was
filtered. Every test passed, and every test ran at most 30 bursts.

While porting the firmware, the C++ host simulator was run with the 3-pulse latency
preset for 15 bursts and the hybrid failed its own acceptance band (36.8 % max
deviation). The Python reference showed the same behaviour, so it was not a port
error. Longer runs of the Python model:

| Preset | Burst 14 | Burst 49 | Burst 199 |
|---|---|---|---|
| standard | 3.6 % | 18.6 % | 177 % |
| latency | 21.2 % | 189 % | 190 % |

The cause is structural. The PID acts inside the burst through the Timelog dead
time, so the table drives the energy through the lifted closed loop `G`, not
through the plant alone. The error propagation of the 1.0 law is `I − αQG`. Its
spectral radius is 1.10 (standard), 1.09 (strong noise) and 1.26 (latency): there is
an unstable high-frequency mode. It starts at noise level, which is why short tests
never saw it.

## Decision

Use the robust ILC form (Bristow, Tharayil, Alleyne, *A survey of iterative learning
control*, IEEE Control Systems Magazine, 2006):

```
u_ff,k+1 = Q(u_ff,k + α·e_k)
```

The propagation becomes `Q(I − αG)`, with spectral radius 0.67–0.74 and 2-norm
0.92 in all presets. The 2-norm below one means monotonic convergence: no learning
transient, whatever the initial table.

## Consequences

* The fixed point is biased where `Q` smooths a sharp feature: −2.2 % on pulse 0
  (the 3-pulse start transient at the burst edge), below 0.5 % on all other pulses.
  The test that demanded 1 % on pulse 0 now demands 2.5 % there and 1 % elsewhere.
* Burst-14 performance is unchanged within noise (3.8 % vs 3.6 % max deviation),
  and it now holds: 3.5 % after 200 bursts instead of 177 %.
* `src/core/stability.py` computes `G` and both contractions. `tests/test_stability.py`
  asserts the contraction for every preset, asserts that the old law would diverge,
  and runs 200 bursts per preset. The C++ suite has the same 200-burst regression
  test, and the golden vectors were regenerated. A control-law change that breaks
  convergence now fails CI instead of the demo.
* Lesson recorded for reviews: a learning controller needs a convergence argument,
  not only a short simulation.
