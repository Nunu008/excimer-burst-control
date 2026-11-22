// SPDX-License-Identifier: MIT
//
// ebc_host_sim - runs the firmware control code on the development machine and
// prints one KPI line per burst. Same output format as the on-target images, so a
// QEMU log and a host log can be diffed directly.
//
//   ebc_host_sim [--bursts N] [--preset standard|noise|latency] [--seed S] [--float]

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <print>
#include <span>
#include <string_view>

#include "ebc/core/noise.hpp"
#include "ebc/sim/lockstep_engine.hpp"

namespace {

struct Options {
  int bursts = 15;
  ebc::Preset preset = ebc::Preset::standard;
  std::uint32_t seed = 42;
  bool single_precision = false;
};

template <typename Int>
bool parse_int(std::string_view text, Int& out) {
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
  return ec == std::errc{} && ptr == text.data() + text.size();
}

bool parse(std::span<char*> args, Options& o) {
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg{args[i]};
    const auto value = [&]() -> std::string_view {
      return i + 1 < args.size() ? std::string_view{args[++i]} : std::string_view{};
    };
    if (arg == "--bursts") {
      if (!parse_int(value(), o.bursts) || o.bursts < 0) return false;
    } else if (arg == "--seed") {
      if (!parse_int(value(), o.seed)) return false;
    } else if (arg == "--preset") {
      const auto name = value();
      if (name == "standard") o.preset = ebc::Preset::standard;
      else if (name == "noise") o.preset = ebc::Preset::strong_gas_noise;
      else if (name == "latency") o.preset = ebc::Preset::high_latency;
      else return false;
    } else if (arg == "--float") {
      o.single_precision = true;
    } else {
      return false;
    }
  }
  return true;
}

template <ebc::Real T>
int run(const Options& o) {
  using Engine = ebc::sim::LockstepEngine<T>;
  const auto params = ebc::make_preset<T>(o.preset);
  auto engine = std::make_unique<Engine>(params);
  auto snapshot = std::make_unique<typename Engine::Snapshot>();
  ebc::PulseNoise<T> noise{params.plant.noise_sigma_rel, o.seed};
  std::array<T, Engine::kPulses> factors{};

  std::println("burst,strategy,max_dev_pct,rms_mj,std_mj,settling_pulse,mean_abs_ufb");
  for (int b = 0; b < o.bursts; ++b) {
    for (T& f : factors) f = noise.next();
    engine->step(factors, *snapshot);
    for (std::size_t s = 0; s < ebc::kStrategyCount; ++s) {
      const auto& r = snapshot->results[s];
      std::println("{},{},{:.3f},{:.4f},{:.4f},{},{:.4f}", snapshot->index,
                   ebc::to_string(static_cast<ebc::Strategy>(s)),
                   static_cast<double>(r.kpis.max_deviation_pct),
                   static_cast<double>(r.kpis.rms_mj), static_cast<double>(r.kpis.std_mj),
                   r.kpis.settling_pulse ? static_cast<long>(*r.kpis.settling_pulse) : -1L,
                   static_cast<double>(r.mean_abs_feedback()));
    }
  }
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse(std::span{argv, static_cast<std::size_t>(argc)}, options)) {
    std::println(stderr,
                 "usage: ebc_host_sim [--bursts N] [--preset standard|noise|latency] "
                 "[--seed S] [--float]");
    return EXIT_FAILURE;
  }
  return options.single_precision ? run<float>(options) : run<double>(options);
}
