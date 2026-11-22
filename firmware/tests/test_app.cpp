// SPDX-License-Identifier: MIT
//
// The application layer of the real-time images, executed on the host: the same
// interrupt-driven pulse loop that the Cortex-R5F and Cortex-M7 run from their
// timer interrupts, with the "interrupt" being a plain function call here.
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "ebc/app/lockstep_demo.hpp"
#include "ebc/app/pulse_loop.hpp"
#include "ebc/app/text.hpp"

namespace {

struct StringSink {
  std::string text;
  void put(char c) { text.push_back(c); }
};

static_assert(ebc::hal::CharSink<StringSink>);

TEST(Text, FixedPointFormatting) {
  const auto fmt = [](double v, unsigned d) {
    StringSink s;
    ebc::app::write_fixed(s, v, d);
    return s.text;
  };
  EXPECT_EQ(fmt(21.7254, 3), "21.725");
  EXPECT_EQ(fmt(0.05531, 4), "0.0553");
  EXPECT_EQ(fmt(-0.25, 2), "-0.25");
  EXPECT_EQ(fmt(-0.0001, 2), "0.00");
  EXPECT_EQ(fmt(3.9996, 3), "4.000");
  EXPECT_EQ(fmt(7.0, 0), "7");
}

TEST(LockstepDemo, PrintsOneLinePerBurstAndStrategyAndPasses) {
  using Engine = ebc::sim::LockstepEngine<float>;
  auto engine = std::make_unique<Engine>(ebc::ControllerParams<float>{});
  auto snapshot = std::make_unique<Engine::Snapshot>();
  StringSink sink;
  const auto verdict = ebc::app::run_lockstep_demo(sink, *engine, *snapshot, 0.01F, 15, 42);
  EXPECT_TRUE(verdict.passed());
  std::size_t lines = 0;
  for (const char c : sink.text) lines += c == '\n' ? 1U : 0U;
  EXPECT_EQ(lines, 1U + 15U * 3U);
  EXPECT_NE(sink.text.find("\n14,hybrid_ilr_pid,"), std::string::npos);
}

class PulseLoopTest : public ::testing::Test {
 protected:
  using Loop = ebc::app::SilPulseLoop<float>;
  std::unique_ptr<Loop> loop = std::make_unique<Loop>(ebc::ControllerParams<float>{}, 42U);
};

TEST_F(PulseLoopTest, ConvergesWhenTheBackgroundLoopKeepsUp) {
  ebc::app::BurstReport<float> report{};
  ebc::app::BurstReport<float> first{};
  bool have_first = false;
  for (std::size_t tick = 0; tick < 20 * Loop::kTicksPerBurst; ++tick) {
    loop->on_tick();  // the timer interrupt
    if (loop->service(report) && !have_first) {
      first = report;
      have_first = true;
    }
  }
  ASSERT_TRUE(have_first);
  EXPECT_EQ(report.burst, 19U);
  EXPECT_EQ(report.learning_overruns, 0U);
  EXPECT_EQ(report.report_overruns, 0U);
  EXPECT_LT(report.kpis.max_deviation_pct, 5.0F);
  EXPECT_LT(report.kpis.rms_mj, 0.4F * first.kpis.rms_mj);
  EXPECT_LT(report.mean_abs_feedback, 0.2F * first.mean_abs_feedback);
}

TEST_F(PulseLoopTest, KeepsRunningAndCountsOverrunsWhenTheBackgroundLoopStalls) {
  for (std::size_t tick = 0; tick < 5 * Loop::kTicksPerBurst; ++tick) loop->on_tick();
  EXPECT_EQ(loop->bursts_completed(), 5U);
  ebc::app::BurstReport<float> report{};
  ASSERT_TRUE(loop->service(report));
  EXPECT_EQ(report.burst, 0U);              // the oldest burst was kept
  EXPECT_EQ(report.learning_overruns, 4U);  // the other four were dropped, not waited for
  EXPECT_EQ(report.report_overruns, 4U);
}

}  // namespace
