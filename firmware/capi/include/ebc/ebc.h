/* SPDX-License-Identifier: MIT
 *
 * ebc.h - C23 interface to the interrupt-safe pulse-energy controller.
 *
 * For C code bases (Xilinx Vitis standalone projects, vendor HALs, existing laser
 * firmware) that cannot take C++ templates. No heap: the caller provides the storage,
 * typically a static buffer in TCM. All functions are reentrant per controller; the
 * ISR/background split is the same as in ebc::RealtimeController:
 *
 *   pulse interrupt : ebc_begin_burst, ebc_on_pulse, ebc_record, ebc_end_burst
 *   background loop : ebc_service_learning
 */
#ifndef EBC_EBC_H
#define EBC_EBC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define EBC_NOEXCEPT noexcept
extern "C" {
#else
#define EBC_NOEXCEPT
#endif

#define EBC_PULSES_PER_BURST 100u
#define EBC_CONTROLLER_STORAGE_SIZE 4096u
#define EBC_CONTROLLER_STORAGE_ALIGN 8u

typedef enum ebc_strategy : uint8_t {
  EBC_OPEN_LOOP = 0,
  EBC_PID_ONLY = 1,
  EBC_HYBRID_ILC_PID = 2,
} ebc_strategy;

typedef enum ebc_status : uint8_t {
  EBC_OK = 0,
  EBC_INVALID_ARGUMENT = 1,
  EBC_STORAGE_TOO_SMALL = 2,
  EBC_INVALID_CONFIG = 3,
} ebc_status;

typedef struct ebc_params {
  float target_energy_mj;
  float nominal_command;        /* open-loop command, initial feedforward table value */
  float kp, ki, kd;
  float integral_limit, output_limit;
  float learning_rate;          /* (0, 1] */
  uint32_t q_filter_depth;      /* >= 1 */
  float command_limit;
} ebc_params;

typedef struct ebc_actuation {
  float command;
  float feedforward;
  float feedback;
} ebc_actuation;

typedef struct ebc_controller ebc_controller; /* opaque */

/* Defaults identical to the Python reference model; nominal_command is computed
 * from the default plant (target / efficiency of pulse 0). */
[[nodiscard]] ebc_params ebc_default_params(void) EBC_NOEXCEPT;

/* Constructs a controller in `storage` (EBC_CONTROLLER_STORAGE_SIZE bytes, aligned to
 * EBC_CONTROLLER_STORAGE_ALIGN). On success *out points into storage. */
[[nodiscard]] ebc_status ebc_controller_init(void* storage, size_t storage_size,
                                             const ebc_params* params, ebc_strategy strategy,
                                             ebc_controller** out) EBC_NOEXCEPT;

/* Pulse-interrupt context: wait-free, constant time. */
void ebc_begin_burst(ebc_controller* controller) EBC_NOEXCEPT;
[[nodiscard]] ebc_actuation ebc_on_pulse(ebc_controller* controller, uint32_t pulse,
                                         float delayed_measurement_mj) EBC_NOEXCEPT;
void ebc_record(ebc_controller* controller, uint32_t pulse, float energy_mj) EBC_NOEXCEPT;
void ebc_end_burst(ebc_controller* controller) EBC_NOEXCEPT;

/* Background context: returns true if a new feedforward table was published. */
bool ebc_service_learning(ebc_controller* controller) EBC_NOEXCEPT;

[[nodiscard]] uint32_t ebc_bursts_learned(const ebc_controller* controller) EBC_NOEXCEPT;
[[nodiscard]] uint32_t ebc_learning_overruns(const ebc_controller* controller) EBC_NOEXCEPT;
[[nodiscard]] float ebc_feedforward(const ebc_controller* controller, uint32_t pulse) EBC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* EBC_EBC_H */
