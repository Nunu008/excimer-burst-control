// SPDX-License-Identifier: MIT
//
// C API over ebc::RealtimeController<float>. The opaque `ebc_controller` is the C++
// object itself, placement-constructed in caller-provided storage.

#include "ebc/ebc.h"

#include <cstdint>
#include <new>

#include "ebc/core/config.hpp"
#include "ebc/core/plant_model.hpp"
#include "ebc/core/realtime_controller.hpp"

struct ebc_controller {
  ebc::RealtimeController<float> impl;
};

namespace {

static_assert(ebc::DefaultGeometry::pulses == EBC_PULSES_PER_BURST);
static_assert(sizeof(ebc_controller) <= EBC_CONTROLLER_STORAGE_SIZE,
              "EBC_CONTROLLER_STORAGE_SIZE in ebc.h is too small");
static_assert(alignof(ebc_controller) <= EBC_CONTROLLER_STORAGE_ALIGN);

ebc::Strategy to_strategy(ebc_strategy s) noexcept {
  switch (s) {
    case EBC_OPEN_LOOP: return ebc::Strategy::open_loop;
    case EBC_PID_ONLY: return ebc::Strategy::pid_only;
    case EBC_HYBRID_ILC_PID: return ebc::Strategy::hybrid_ilc_pid;
  }
  return ebc::Strategy::hybrid_ilc_pid;
}

ebc::ControllerParams<float> to_params(const ebc_params& p) noexcept {
  ebc::ControllerParams<float> params{};
  params.plant.target_energy_mj = p.target_energy_mj;
  params.pid = {.kp = p.kp,
                .ki = p.ki,
                .kd = p.kd,
                .integral_limit = p.integral_limit,
                .output_limit = p.output_limit};
  params.ilc = {.learning_rate = p.learning_rate, .q_filter_depth = p.q_filter_depth};
  params.command_limit = p.command_limit;
  return params;
}

}  // namespace

extern "C" {

ebc_params ebc_default_params(void) noexcept {
  const ebc::ControllerParams<float> d{};
  return {.target_energy_mj = d.plant.target_energy_mj,
          .nominal_command =
              d.plant.target_energy_mj / (d.plant.actuator_gain * ebc::gas_efficiency(d.plant, 0)),
          .kp = d.pid.kp,
          .ki = d.pid.ki,
          .kd = d.pid.kd,
          .integral_limit = d.pid.integral_limit,
          .output_limit = d.pid.output_limit,
          .learning_rate = d.ilc.learning_rate,
          .q_filter_depth = static_cast<std::uint32_t>(d.ilc.q_filter_depth),
          .command_limit = d.command_limit};
}

ebc_status ebc_controller_init(void* storage, size_t storage_size, const ebc_params* params,
                               ebc_strategy strategy, ebc_controller** out) noexcept {
  if (storage == nullptr || params == nullptr || out == nullptr) return EBC_INVALID_ARGUMENT;
  if (storage_size < sizeof(ebc_controller) ||
      reinterpret_cast<std::uintptr_t>(storage) % alignof(ebc_controller) != 0) {
    return EBC_STORAGE_TOO_SMALL;
  }
  const auto cpp_params = to_params(*params);
  if (!ebc::validate(cpp_params) || !(params->nominal_command > 0.0F)) return EBC_INVALID_CONFIG;
  *out = ::new (storage) ebc_controller{
      ebc::RealtimeController<float>{cpp_params, params->nominal_command, to_strategy(strategy)}};
  return EBC_OK;
}

void ebc_begin_burst(ebc_controller* c) noexcept { c->impl.begin_burst(); }

ebc_actuation ebc_on_pulse(ebc_controller* c, uint32_t pulse, float measurement) noexcept {
  const auto a = c->impl.on_pulse(pulse, measurement);
  return {.command = a.command, .feedforward = a.feedforward, .feedback = a.feedback};
}

void ebc_record(ebc_controller* c, uint32_t pulse, float energy) noexcept {
  c->impl.record(pulse, energy);
}

void ebc_end_burst(ebc_controller* c) noexcept { c->impl.end_burst(); }
bool ebc_service_learning(ebc_controller* c) noexcept { return c->impl.service_learning(); }
uint32_t ebc_bursts_learned(const ebc_controller* c) noexcept { return c->impl.bursts_learned(); }
uint32_t ebc_learning_overruns(const ebc_controller* c) noexcept {
  return c->impl.learning_overruns();
}
float ebc_feedforward(const ebc_controller* c, uint32_t pulse) noexcept {
  return c->impl.published_table()[pulse];
}

}  // extern "C"
