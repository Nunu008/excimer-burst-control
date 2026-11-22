// SPDX-License-Identifier: MIT
//
// ebc_host_sim - runs the firmware application on the development machine. It
// calls the same `run_lockstep_demo` as the Cortex-A53 image, so the host log and
// the QEMU log have the identical format and can be compared number by number.
//
//   ebc_host_sim [--bursts N] [--preset standard|noise|latency] [--seed S] [--float]

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string_view>

#include "ebc/app/lockstep_demo.hpp"

namespace {

struct StdoutSink {
  void put(char c) { std::fputc(c, stdout); }
};

struct Options {
  std::uint32_t bursts = 15;
  ebc::Preset preset = ebc::Preset::standard;
  std::uint32_t seed = 42;
  bool single_precision = false;
};

template <typename Int>
bool parse_int(std::string_view text, Int& out) {
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
  return ec == std::errc{} && ptr == text.data() + text.size() && !text.empty();
}

bool parse(std::span<char*> args, Options& o) {
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg{args[i]};
    const std::string_view value = i + 1 < args.size() ? std::string_view{args[i + 1]} : "";
    if (arg == "--float") {
      o.single_precision = true;
      continue;
    }
    ++i;
    if (arg == "--bursts") {
      if (!parse_int(value, o.bursts)) return false;
    } else if (arg == "--seed") {
      if (!parse_int(value, o.seed)) return false;
    } else if (arg == "--preset") {
      if (value == "standard") o.preset = ebc::Preset::standard;
      else if (value == "noise") o.preset = ebc::Preset::strong_gas_noise;
      else if (value == "latency") o.preset = ebc::Preset::high_latency;
      else return false;
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
  StdoutSink sink;
  const auto verdict = ebc::app::run_lockstep_demo(sink, *engine, *snapshot,
                                                   params.plant.noise_sigma_rel, o.bursts, o.seed);
  return verdict.passed() || o.bursts < 10 ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  if (!parse(std::span{argv, static_cast<std::size_t>(argc)}, options)) {
    std::fputs("usage: ebc_host_sim [--bursts N] [--preset standard|noise|latency] "
               "[--seed S] [--float]\n",
               stderr);
    return EXIT_FAILURE;
  }
  return options.single_precision ? run<float>(options) : run<double>(options);
}
