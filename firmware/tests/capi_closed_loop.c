// SPDX-License-Identifier: MIT
//
// A plain C23 program that uses the controller through ebc.h, the way an existing C
// laser firmware would: static storage, its own plant model, its own dead time.
// Exit code 0 if the hybrid converges and learning never overran.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ebc/ebc.h"

constexpr uint32_t pulses = EBC_PULSES_PER_BURST;
constexpr uint32_t delay = 2;
constexpr int bursts = 25;

static float efficiency(uint32_t i) {
  const float x = (float)i;
  return expf(-0.004F * x) * (1.0F - 0.15F * expf(-x / 3.0F));
}

alignas(EBC_CONTROLLER_STORAGE_ALIGN) static unsigned char storage[EBC_CONTROLLER_STORAGE_SIZE];

int main(void) {
  const ebc_params params = ebc_default_params();
  ebc_controller* ctl = nullptr;
  if (ebc_controller_init(storage, sizeof storage, &params, EBC_HYBRID_ILC_PID, &ctl) != EBC_OK) {
    fputs("init failed\n", stderr);
    return EXIT_FAILURE;
  }

  // Invalid input is rejected, not trusted.
  ebc_params bad = params;
  bad.learning_rate = 1.5F;
  ebc_controller* unused = nullptr;
  alignas(8) static unsigned char other[EBC_CONTROLLER_STORAGE_SIZE];
  if (ebc_controller_init(other, sizeof other, &bad, EBC_HYBRID_ILC_PID, &unused) !=
      EBC_INVALID_CONFIG) {
    fputs("invalid learning rate accepted\n", stderr);
    return EXIT_FAILURE;
  }

  float worst_last = 0.0F;
  for (int b = 0; b < bursts; ++b) {
    float energy[EBC_PULSES_PER_BURST] = {};
    float worst = 0.0F;
    ebc_begin_burst(ctl);
    for (uint32_t i = 0; i < pulses; ++i) {
      const float measured = i >= delay ? energy[i - delay] : params.target_energy_mj;
      const ebc_actuation a = ebc_on_pulse(ctl, i, measured);
      energy[i] = a.command * efficiency(i);
      ebc_record(ctl, i, energy[i]);
      const float dev = fabsf(energy[i] - params.target_energy_mj) / params.target_energy_mj;
      worst = dev > worst ? dev : worst;
    }
    ebc_end_burst(ctl);
    (void)ebc_service_learning(ctl);
    worst_last = worst;
  }

  printf("C API: %u bursts learned, %u overruns, last-burst max deviation %.2f %%\n",
         (unsigned)ebc_bursts_learned(ctl), (unsigned)ebc_learning_overruns(ctl),
         (double)(worst_last * 100.0F));
  const bool ok = worst_last < 0.05F && ebc_learning_overruns(ctl) == 0 &&
                  ebc_bursts_learned(ctl) == (uint32_t)bursts;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
