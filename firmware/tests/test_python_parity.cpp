// SPDX-License-Identifier: MIT
//
// Cross-language parity (ADR 0002). Golden vectors exported from the Python model
// by scripts/export_golden.py are replayed through the C++ core with the very same
// noise realisation. Every energy, every actuator value and every KPI of every
// strategy must match:
//
//   double: to 1e-9 relative - the same algorithm, only rounding order differs
//   float : to 5e-4 relative - the single-precision target build, 15 bursts of
//           learning accumulated
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ebc/sim/lockstep_engine.hpp"

namespace {

using ebc::Strategy;
constexpr std::size_t kPulses = ebc::DefaultGeometry::pulses;

std::vector<std::vector<std::string>> read_csv(const std::string& name) {
  std::ifstream file{std::string{EBC_GOLDEN_DIR} + "/" + name};
  if (!file) ADD_FAILURE() << "cannot open golden file " << name;
  std::vector<std::vector<std::string>> rows;
  std::string line;
  std::getline(file, line);  // header
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> cells;
    std::stringstream ss{line};
    for (std::string cell; std::getline(ss, cell, ',');) cells.push_back(cell);
    rows.push_back(std::move(cells));
  }
  return rows;
}

double num(const std::string& s) {
  double value = 0.0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  EXPECT_EQ(ec, std::errc{}) << s;
  return value;
}

struct Case {
  const char* preset;
  ebc::Preset id;

  friend void PrintTo(const Case& c, std::ostream* os) { *os << c.preset; }
};

constexpr std::array kCases{Case{"standard", ebc::Preset::standard},
                            Case{"noise", ebc::Preset::strong_gas_noise},
                            Case{"latency", ebc::Preset::high_latency}};

void expect_rel(double actual, double expected, double rel, const char* what, std::size_t burst,
                std::size_t pulse) {
  const double tol = rel * std::max(1.0, std::abs(expected));
  ASSERT_NEAR(actual, expected, tol) << what << " burst " << burst << " pulse " << pulse;
}

template <typename T>
void replay(const Case& c, double rel) {
  const auto pulses = read_csv(std::string{c.preset} + "_pulses.csv");
  const auto kpis = read_csv(std::string{c.preset} + "_kpis.csv");
  ASSERT_FALSE(pulses.empty());
  ASSERT_EQ(pulses.size() % kPulses, 0U);
  const std::size_t bursts = pulses.size() / kPulses;

  using Engine = ebc::sim::LockstepEngine<T>;
  auto engine = std::make_unique<Engine>(ebc::make_preset<T>(c.id));
  auto snap = std::make_unique<typename Engine::Snapshot>();

  for (std::size_t b = 0; b < bursts; ++b) {
    std::array<T, kPulses> noise{};
    for (std::size_t i = 0; i < kPulses; ++i) {
      noise[i] = static_cast<T>(num(pulses[b * kPulses + i][2]));
    }
    engine->step(noise, *snap);

    const auto& ol = snap->result(Strategy::open_loop);
    const auto& pid = snap->result(Strategy::pid_only);
    const auto& hyb = snap->result(Strategy::hybrid_ilc_pid);
    for (std::size_t i = 0; i < kPulses; ++i) {
      const auto& row = pulses[b * kPulses + i];
      expect_rel(ol.energy_mj[i], num(row[3]), rel, "E open loop", b, i);
      expect_rel(pid.energy_mj[i], num(row[4]), rel, "E pid", b, i);
      expect_rel(hyb.energy_mj[i], num(row[5]), rel, "E hybrid", b, i);
      expect_rel(pid.command[i], num(row[6]), rel, "u pid", b, i);
      expect_rel(hyb.feedforward[i], num(row[7]), rel, "u_ff hybrid", b, i);
      expect_rel(hyb.feedback[i], num(row[8]), rel, "u_fb hybrid", b, i);
    }

    for (std::size_t s = 0; s < ebc::kStrategyCount; ++s) {
      const auto& row = kpis[b * ebc::kStrategyCount + s];
      const auto& k = snap->results[s].kpis;
      ASSERT_EQ(row[1], ebc::to_string(static_cast<Strategy>(s)));
      expect_rel(k.max_deviation_pct, num(row[2]), rel * 100, "max dev %", b, s);
      expect_rel(k.std_mj, num(row[3]), rel, "std", b, s);
      expect_rel(k.rms_mj, num(row[4]), rel, "rms", b, s);
      if constexpr (std::is_same_v<T, double>) {
        // Settling is a threshold decision; exact agreement is only meaningful in
        // the same precision as the reference.
        const long expected_settling = std::stol(row[5]);
        const long actual = k.settling_pulse ? static_cast<long>(*k.settling_pulse) : -1L;
        ASSERT_EQ(actual, expected_settling) << "settling, burst " << b << " mode " << s;
      }
    }
  }
}

class PythonParity : public ::testing::TestWithParam<Case> {};

TEST_P(PythonParity, DoublePrecisionMatchesPythonBitForBitWithinRounding) {
  replay<double>(GetParam(), 1e-9);
}

TEST_P(PythonParity, SinglePrecisionTargetBuildStaysWithinBound) {
  replay<float>(GetParam(), 5e-4);
}

INSTANTIATE_TEST_SUITE_P(AllPresets, PythonParity, ::testing::ValuesIn(kCases),
                         [](const auto& param_info) { return std::string{param_info.param.preset}; });

}  // namespace
