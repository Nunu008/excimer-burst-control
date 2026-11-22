// SPDX-License-Identifier: MIT
//
// One CSV line per burst and strategy, byte-identical in format to ebc_host_sim so
// that a QEMU log and a host log can be compared by tools/compare_logs.py.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ebc/app/text.hpp"
#include "ebc/core/hybrid.hpp"
#include "ebc/core/metrics.hpp"

namespace ebc::app {

template <hal::CharSink S>
void write_header(S& sink) noexcept {
  write(sink, "burst,strategy,max_dev_pct,rms_mj,std_mj,settling_pulse,mean_abs_ufb\n");
}

template <hal::CharSink S, Real T>
void write_burst_line(S& sink, std::uint32_t burst, Strategy strategy, const Kpis<T>& kpis,
                      T mean_abs_feedback) noexcept {
  write_int(sink, burst);
  sink.put(',');
  write(sink, to_string(strategy));
  sink.put(',');
  write_fixed(sink, kpis.max_deviation_pct, 3);
  sink.put(',');
  write_fixed(sink, kpis.rms_mj, 4);
  sink.put(',');
  write_fixed(sink, kpis.std_mj, 4);
  sink.put(',');
  if (kpis.settling_pulse) {
    write_int(sink, *kpis.settling_pulse);
  } else {
    write(sink, "-1");
  }
  sink.put(',');
  write_fixed(sink, mean_abs_feedback, 4);
  sink.put('\n');
}

}  // namespace ebc::app
