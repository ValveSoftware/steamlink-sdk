// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_power_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;

namespace content {

namespace {

const int kRenderProcessId = 1;
const int kAnotherRenderProcessId = 2;
const int kStreamId = 3;
const int kAnotherStreamId = 6;

// Used to confirm audio indicator state changes occur at the correct times.
class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD2(NavigationStateChanged,
               void(WebContents* source, InvalidateTypes changed_flags));
};

}  // namespace

class AudioStreamMonitorTest : public RenderViewHostTestHarness {
 public:
  AudioStreamMonitorTest() {
    // Start |clock_| at non-zero.
    clock_.Advance(base::TimeDelta::FromSeconds(1000000));
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    WebContentsImpl* web_contents = reinterpret_cast<WebContentsImpl*>(
        RenderViewHostTestHarness::web_contents());
    web_contents->SetDelegate(&mock_web_contents_delegate_);

    monitor_ = web_contents->audio_stream_monitor();
    const_cast<base::TickClock*&>(monitor_->clock_) = &clock_;
  }

  base::TimeTicks GetTestClockTime() { return clock_.NowTicks(); }

  void AdvanceClock(const base::TimeDelta& delta) { clock_.Advance(delta); }

  AudioStreamMonitor::ReadPowerAndClipCallback CreatePollCallback(
      int stream_id) {
    return base::Bind(
        &AudioStreamMonitorTest::ReadPower, base::Unretained(this), stream_id);
  }

  void SetStreamPower(int stream_id, float power) {
    current_power_[stream_id] = power;
  }

  void SimulatePollTimerFired() { monitor_->Poll(); }

  void SimulateOffTimerFired() { monitor_->MaybeToggle(); }

  void ExpectIsPolling(int render_process_id, int stream_id, bool is_polling) {
    const AudioStreamMonitor::StreamID key(render_process_id, stream_id);
    EXPECT_EQ(
        is_polling,
        monitor_->poll_callbacks_.find(key) != monitor_->poll_callbacks_.end());
    EXPECT_EQ(!monitor_->poll_callbacks_.empty(),
              monitor_->poll_timer_.IsRunning());
  }

  void ExpectTabWasRecentlyAudible(bool was_audible,
                                   const base::TimeTicks& last_blurt_time) {
    EXPECT_EQ(was_audible, monitor_->was_recently_audible_);
    EXPECT_EQ(last_blurt_time, monitor_->last_blurt_time_);
    EXPECT_EQ(monitor_->was_recently_audible_,
              monitor_->off_timer_.IsRunning());
  }

  void ExpectIsCurrentlyAudible() const {
    EXPECT_TRUE(monitor_->IsCurrentlyAudible());
  }

  void ExpectNotCurrentlyAudible() const {
    EXPECT_FALSE(monitor_->IsCurrentlyAudible());
  }

  void ExpectRecentlyAudibleChangeNotification(bool new_recently_audible) {
    EXPECT_CALL(
        mock_web_contents_delegate_,
        NavigationStateChanged(RenderViewHostTestHarness::web_contents(),
                               INVALIDATE_TYPE_TAB))
        .WillOnce(InvokeWithoutArgs(
            this,
            new_recently_audible
                ? &AudioStreamMonitorTest::ExpectWasRecentlyAudible
                : &AudioStreamMonitorTest::ExpectNotRecentlyAudible))
        .RetiresOnSaturation();
  }

  void ExpectCurrentlyAudibleChangeNotification(bool new_audible) {
    EXPECT_CALL(
        mock_web_contents_delegate_,
        NavigationStateChanged(RenderViewHostTestHarness::web_contents(),
                               INVALIDATE_TYPE_TAB))
        .WillOnce(InvokeWithoutArgs(
            this,
            new_audible
                ? &AudioStreamMonitorTest::ExpectIsCurrentlyAudible
                : &AudioStreamMonitorTest::ExpectNotCurrentlyAudible))
        .RetiresOnSaturation();
  }

  static base::TimeDelta one_polling_interval() {
    return base::TimeDelta::FromSeconds(1) /
           static_cast<int>(AudioStreamMonitor::kPowerMeasurementsPerSecond);
  }

  static base::TimeDelta holding_period() {
    return base::TimeDelta::FromMilliseconds(
        AudioStreamMonitor::kHoldOnMilliseconds);
  }

  void StartMonitoring(
      int render_process_id,
      int stream_id,
      const AudioStreamMonitor::ReadPowerAndClipCallback& callback) {
    monitor_->StartMonitoringStreamOnUIThread(
        render_process_id, stream_id, callback);
  }

  void StopMonitoring(int render_process_id, int stream_id) {
    monitor_->StopMonitoringStreamOnUIThread(render_process_id, stream_id);
  }

 protected:
  AudioStreamMonitor* monitor_;

 private:
  std::pair<float, bool> ReadPower(int stream_id) {
    return std::make_pair(current_power_[stream_id], false);
  }

  void ExpectWasRecentlyAudible() const {
    EXPECT_TRUE(monitor_->WasRecentlyAudible());
  }

  void ExpectNotRecentlyAudible() const {
    EXPECT_FALSE(monitor_->WasRecentlyAudible());
  }

  MockWebContentsDelegate mock_web_contents_delegate_;
  base::SimpleTestTickClock clock_;
  std::map<int, float> current_power_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamMonitorTest);
};

// Tests that AudioStreamMonitor is polling while it has a
// ReadPowerAndClipCallback, and is not polling at other times.
TEST_F(AudioStreamMonitorTest, PollsWhenProvidedACallback) {
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsPolling(kRenderProcessId, kStreamId, false);

  StartMonitoring(kRenderProcessId, kStreamId, CreatePollCallback(kStreamId));
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsPolling(kRenderProcessId, kStreamId, true);

  StopMonitoring(kRenderProcessId, kStreamId);
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsPolling(kRenderProcessId, kStreamId, false);
}

// Tests that AudioStreamMonitor debounces the power level readings it's taking,
// which could be fluctuating rapidly between the audible versus silence
// threshold.  See comments in audio_stream_monitor.h for expected behavior.
TEST_F(AudioStreamMonitorTest,
       ImpulsesKeepIndicatorOnUntilHoldingPeriodHasPassed) {
  StartMonitoring(kRenderProcessId, kStreamId, CreatePollCallback(kStreamId));

  // Expect WebContents will get one call form AudioStreamMonitor to toggle the
  // indicator on upon the very first poll.
  ExpectRecentlyAudibleChangeNotification(true);

  // Loop, each time testing a slightly longer period of polled silence.  The
  // recently audible state should not change while the currently audible one
  // should.
  int num_silence_polls = 1;
  base::TimeTicks last_blurt_time;
  do {
    ExpectCurrentlyAudibleChangeNotification(true);

    // Poll an audible signal, and expect tab indicator state is on.
    SetStreamPower(kStreamId, media::AudioPowerMonitor::max_power());
    last_blurt_time = GetTestClockTime();
    SimulatePollTimerFired();
    ExpectTabWasRecentlyAudible(true, last_blurt_time);
    AdvanceClock(one_polling_interval());

    ExpectCurrentlyAudibleChangeNotification(false);

    // Poll a silent signal repeatedly, ensuring that the indicator is being
    // held on during the holding period.
    SetStreamPower(kStreamId, media::AudioPowerMonitor::zero_power());
    for (int i = 0; i < num_silence_polls; ++i) {
      SimulatePollTimerFired();
      ExpectTabWasRecentlyAudible(true, last_blurt_time);
      // Note: Redundant off timer firings should not have any effect.
      SimulateOffTimerFired();
      ExpectTabWasRecentlyAudible(true, last_blurt_time);
      AdvanceClock(one_polling_interval());
    }

    ++num_silence_polls;
  } while (GetTestClockTime() < last_blurt_time + holding_period());

  // At this point, the clock has just advanced to beyond the holding period, so
  // the next firing of the off timer should turn off the tab indicator.  Also,
  // make sure it stays off for several cycles thereafter.
  ExpectRecentlyAudibleChangeNotification(false);
  for (int i = 0; i < 10; ++i) {
    SimulateOffTimerFired();
    ExpectTabWasRecentlyAudible(false, last_blurt_time);
    AdvanceClock(one_polling_interval());
  }
}

// Tests that the AudioStreamMonitor correctly processes the blurts from two
// different streams in the same tab.
TEST_F(AudioStreamMonitorTest, HandlesMultipleStreamsBlurting) {
  StartMonitoring(kRenderProcessId, kStreamId, CreatePollCallback(kStreamId));
  StartMonitoring(
      kRenderProcessId, kAnotherStreamId, CreatePollCallback(kAnotherStreamId));

  base::TimeTicks last_blurt_time;
  ExpectTabWasRecentlyAudible(false, last_blurt_time);
  ExpectNotCurrentlyAudible();

  // Measure audible sound from the first stream and silence from the second.
  // The tab becomes audible.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);

  SetStreamPower(kStreamId, media::AudioPowerMonitor::max_power());
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);
  ExpectIsCurrentlyAudible();

  // Halfway through the holding period, the second stream joins in.  The
  // indicator stays on.
  AdvanceClock(holding_period() / 2);
  SimulateOffTimerFired();
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::max_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();  // Restarts holding period.
  ExpectTabWasRecentlyAudible(true, last_blurt_time);
  ExpectIsCurrentlyAudible();

  // Now, measure silence from both streams.  After an entire holding period
  // has passed (since the second stream joined in), the tab will no longer
  // become audible nor recently audible.
  ExpectCurrentlyAudibleChangeNotification(false);
  ExpectRecentlyAudibleChangeNotification(false);

  AdvanceClock(holding_period());
  SimulateOffTimerFired();
  SetStreamPower(kStreamId, media::AudioPowerMonitor::zero_power());
  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(false, last_blurt_time);
  ExpectNotCurrentlyAudible();

  // Now, measure silence from the first stream and audible sound from the
  // second.  The tab becomes audible again.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);

  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::max_power());
  last_blurt_time = GetTestClockTime();
  SimulatePollTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);
  ExpectIsCurrentlyAudible();

  // From here onwards, both streams are silent.  Halfway through the holding
  // period, the tab is no longer audible but stays as recently audible.
  ExpectCurrentlyAudibleChangeNotification(false);

  SetStreamPower(kAnotherStreamId, media::AudioPowerMonitor::zero_power());
  AdvanceClock(holding_period() / 2);
  SimulatePollTimerFired();
  SimulateOffTimerFired();
  ExpectTabWasRecentlyAudible(true, last_blurt_time);
  ExpectNotCurrentlyAudible();

  // Just past the holding period, the tab is no longer marked as recently
  // audible.
  ExpectRecentlyAudibleChangeNotification(false);

  AdvanceClock(holding_period() - (GetTestClockTime() - last_blurt_time));
  SimulateOffTimerFired();
  ExpectTabWasRecentlyAudible(false, last_blurt_time);
  ExpectNotCurrentlyAudible();

  // Polling should not turn the indicator back while both streams are remaining
  // silent.
  for (int i = 0; i < 100; ++i) {
    AdvanceClock(one_polling_interval());
    SimulatePollTimerFired();
    ExpectTabWasRecentlyAudible(false, last_blurt_time);
    ExpectNotCurrentlyAudible();
  }
}

TEST_F(AudioStreamMonitorTest, MultipleRendererProcesses) {
  StartMonitoring(kRenderProcessId, kStreamId, CreatePollCallback(kStreamId));
  StartMonitoring(
      kAnotherRenderProcessId, kStreamId, CreatePollCallback(kStreamId));
  ExpectIsPolling(kRenderProcessId, kStreamId, true);
  ExpectIsPolling(kAnotherRenderProcessId, kStreamId, true);
  StopMonitoring(kAnotherRenderProcessId, kStreamId);
  ExpectIsPolling(kRenderProcessId, kStreamId, true);
  ExpectIsPolling(kAnotherRenderProcessId, kStreamId, false);
}

}  // namespace content
