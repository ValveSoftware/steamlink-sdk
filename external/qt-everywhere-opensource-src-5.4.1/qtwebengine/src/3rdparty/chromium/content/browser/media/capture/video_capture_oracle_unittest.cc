// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/video_capture_oracle.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void SteadyStateSampleAndAdvance(base::TimeDelta vsync,
                                 SmoothEventSampler* sampler,
                                 base::TimeTicks* t) {
  ASSERT_TRUE(sampler->AddEventAndConsiderSampling(*t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  sampler->RecordSample();
  ASSERT_FALSE(sampler->HasUnrecordedEvent());
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
  *t += vsync;
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
}

void SteadyStateNoSampleAndAdvance(base::TimeDelta vsync,
                                   SmoothEventSampler* sampler,
                                   base::TimeTicks* t) {
  ASSERT_FALSE(sampler->AddEventAndConsiderSampling(*t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
  *t += vsync;
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t));
}

void TimeTicksFromString(const char* string, base::TimeTicks* t) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(string, &time));
  *t = base::TimeTicks::UnixEpoch() + (time - base::Time::UnixEpoch());
}

void TestRedundantCaptureStrategy(base::TimeDelta capture_period,
                                  int redundant_capture_goal,
                                  SmoothEventSampler* sampler,
                                  base::TimeTicks* t) {
  // Before any events have been considered, we're overdue for sampling.
  ASSERT_TRUE(sampler->IsOverdueForSamplingAt(*t));

  // Consider the first event.  We want to sample that.
  ASSERT_FALSE(sampler->HasUnrecordedEvent());
  ASSERT_TRUE(sampler->AddEventAndConsiderSampling(*t));
  ASSERT_TRUE(sampler->HasUnrecordedEvent());
  sampler->RecordSample();
  ASSERT_FALSE(sampler->HasUnrecordedEvent());

  // After more than one capture period has passed without considering an event,
  // we should repeatedly be overdue for sampling.  However, once the redundant
  // capture goal is achieved, we should no longer be overdue for sampling.
  *t += capture_period * 4;
  for (int i = 0; i < redundant_capture_goal; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_FALSE(sampler->HasUnrecordedEvent());
    ASSERT_TRUE(sampler->IsOverdueForSamplingAt(*t))
        << "Should sample until redundant capture goal is hit";
    sampler->RecordSample();
    *t += capture_period;  // Timer fires once every capture period.
  }
  ASSERT_FALSE(sampler->IsOverdueForSamplingAt(*t))
      << "Should not be overdue once redundant capture goal achieved.";
}

// 60Hz sampled at 30Hz should produce 30Hz.  In addition, this test contains
// much more comprehensive before/after/edge-case scenarios than the others.
TEST(SmoothEventSamplerTest, Sample60HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 200;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 60;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every other vsync, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 7, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    ASSERT_TRUE(sampler.HasUnrecordedEvent());
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state,
  // but at a different phase.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 50Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample50HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 2;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 50;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture 1st, 2nd and 4th frames out of every five
  // frames, indefinitely.
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 12; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 5, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state
  // again.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 75Hz sampled at 30Hz should produce a sequence where some frames are skipped.
TEST(SmoothEventSamplerTest, Sample75HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 32;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 75;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture 1st and 3rd frames out of every five
  // frames, indefinitely.
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 20; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 8, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We capture the next frame, and not the one
  // after that, and then we're back in the steady state again.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
    SteadyStateNoSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 30Hz sampled at 30Hz should produce 30Hz.
TEST(SmoothEventSamplerTest, Sample30HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 1;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 30;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 3, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

// 24Hz sampled at 30Hz should produce 24Hz.
TEST(SmoothEventSamplerTest, Sample24HertzAt30Hertz) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const int redundant_capture_goal = 333;
  const base::TimeDelta vsync = base::TimeDelta::FromSeconds(1) / 24;

  SmoothEventSampler sampler(capture_period, true, redundant_capture_goal);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  TestRedundantCaptureStrategy(capture_period, redundant_capture_goal,
                               &sampler, &t);

  // Steady state, we should capture every vsync, indefinitely.
  for (int i = 0; i < 200; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }

  // Now pretend we're limited by backpressure in the pipeline. In this scenario
  // case we are adding events but not sampling them.
  for (int i = 0; i < 7; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    ASSERT_EQ(i >= 3, sampler.IsOverdueForSamplingAt(t));
    ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
    t += vsync;
  }

  // Now suppose we can sample again. We should be back in the steady state.
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t));
  for (int i = 0; i < 100; i++) {
    SCOPED_TRACE(base::StringPrintf("Iteration %d", i));
    SteadyStateSampleAndAdvance(vsync, &sampler, &t);
  }
}

TEST(SmoothEventSamplerTest, DoubleDrawAtOneTimeStillDirties) {
  const base::TimeDelta capture_period = base::TimeDelta::FromSeconds(1) / 30;
  const base::TimeDelta overdue_period = base::TimeDelta::FromSeconds(1);

  SmoothEventSampler sampler(capture_period, true, 1);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
  sampler.RecordSample();
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  t += overdue_period;

  // Now simulate 2 events with the same clock value.
  ASSERT_TRUE(sampler.AddEventAndConsiderSampling(t));
  sampler.RecordSample();
  ASSERT_FALSE(sampler.AddEventAndConsiderSampling(t))
      << "Two events at same time -- expected second not to be sampled.";
  ASSERT_TRUE(sampler.IsOverdueForSamplingAt(t + overdue_period))
      << "Second event should dirty the capture state.";
  sampler.RecordSample();
  ASSERT_FALSE(sampler.IsOverdueForSamplingAt(t + overdue_period));
}

TEST(SmoothEventSamplerTest, FallbackToPollingIfUpdatesUnreliable) {
  const base::TimeDelta timer_interval = base::TimeDelta::FromSeconds(1) / 30;

  SmoothEventSampler should_not_poll(timer_interval, true, 1);
  SmoothEventSampler should_poll(timer_interval, false, 1);
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);

  // Do one round of the "happy case" where an event was received and
  // RecordSample() was called by the client.
  ASSERT_TRUE(should_not_poll.AddEventAndConsiderSampling(t));
  ASSERT_TRUE(should_poll.AddEventAndConsiderSampling(t));
  should_not_poll.RecordSample();
  should_poll.RecordSample();

  // One time period ahead, neither sampler says we're overdue.
  for (int i = 0; i < 3; i++) {
    t += timer_interval;
    ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
        << "Sampled last event; should not be dirty.";
    ASSERT_FALSE(should_poll.IsOverdueForSamplingAt(t))
        << "Dirty interval has not elapsed yet.";
  }

  // Next time period ahead, both samplers say we're overdue.  The non-polling
  // sampler is returning true here because it has been configured to allow one
  // redundant capture.
  t += timer_interval;
  ASSERT_TRUE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; is dirty one time only to meet redundancy goal.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_not_poll.RecordSample();
  should_poll.RecordSample();

  // Forever more, the non-polling sampler returns false while the polling one
  // returns true.
  for (int i = 0; i < 100; ++i) {
    t += timer_interval;
    ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
        << "Sampled last event; should not be dirty.";
    ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
        << "If updates are unreliable, must fall back to polling when idle.";
    should_poll.RecordSample();
  }
  t += timer_interval / 3;
  ASSERT_FALSE(should_not_poll.IsOverdueForSamplingAt(t))
      << "Sampled last event; should not be dirty.";
  ASSERT_TRUE(should_poll.IsOverdueForSamplingAt(t))
      << "If updates are unreliable, must fall back to polling when idle.";
  should_poll.RecordSample();
}

struct DataPoint {
  bool should_capture;
  double increment_ms;
};

void ReplayCheckingSamplerDecisions(const DataPoint* data_points,
                                    size_t num_data_points,
                                    SmoothEventSampler* sampler) {
  base::TimeTicks t;
  TimeTicksFromString("Sat, 23 Mar 2013 1:21:08 GMT", &t);
  for (size_t i = 0; i < num_data_points; ++i) {
    t += base::TimeDelta::FromMicroseconds(
        static_cast<int64>(data_points[i].increment_ms * 1000));
    ASSERT_EQ(data_points[i].should_capture,
              sampler->AddEventAndConsiderSampling(t))
        << "at data_points[" << i << ']';
    if (data_points[i].should_capture)
      sampler->RecordSample();
  }
}

TEST(SmoothEventSamplerTest, DrawingAt24FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 24 FPS video was
  // started from a still screen, then clearly followed by steady-state.
  static const DataPoint data_points[] = {
    { true, 1437.93 }, { true, 150.484 }, { true, 217.362 }, { true, 50.161 },
    { true, 33.44 }, { false, 0 }, { true, 16.721 }, { true, 66.88 },
    { true, 50.161 }, { false, 0 }, { false, 0 }, { true, 50.16 },
    { true, 33.441 }, { true, 16.72 }, { false, 16.72 }, { true, 117.041 },
    { true, 16.72 }, { false, 16.72 }, { true, 50.161 }, { true, 50.16 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { true, 16.72 },
    { false, 0 }, { true, 50.161 }, { false, 0 }, { true, 33.44 },
    { true, 16.72 }, { false, 16.721 }, { true, 66.881 }, { false, 0 },
    { true, 33.441 }, { true, 16.72 }, { true, 50.16 }, { true, 16.72 },
    { false, 16.721 }, { true, 50.161 }, { true, 50.16 }, { false, 0 },
    { true, 33.441 }, { true, 50.337 }, { true, 50.183 }, { true, 16.722 },
    { true, 50.161 }, { true, 33.441 }, { true, 50.16 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.441 }, { true, 50.16 }, { true, 33.44 },
    { true, 50.161 }, { true, 50.16 }, { true, 33.44 }, { true, 33.441 },
    { true, 50.16 }, { true, 50.161 }, { true, 33.44 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.44 }, { true, 50.161 }, { true, 33.44 },
    { true, 50.161 }, { true, 33.44 }, { true, 50.161 }, { true, 33.44 },
    { true, 83.601 }, { true, 16.72 }, { true, 33.44 }, { false, 0 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt30FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: Initial instability as a 30 FPS video was
  // started from a still screen, then followed by steady-state.  Drawing
  // framerate from the video rendering was a bit volatile, but averaged 30 FPS.
  static const DataPoint data_points[] = {
    { true, 2407.69 }, { true, 16.733 }, { true, 217.362 }, { true, 33.441 },
    { true, 33.44 }, { true, 33.44 }, { true, 33.441 }, { true, 33.44 },
    { true, 33.44 }, { true, 33.441 }, { true, 33.44 }, { true, 33.44 },
    { true, 16.721 }, { true, 33.44 }, { false, 0 }, { true, 50.161 },
    { true, 50.16 }, { false, 0 }, { true, 50.161 }, { true, 33.44 },
    { true, 16.72 }, { false, 0 }, { false, 16.72 }, { true, 66.881 },
    { false, 0 }, { true, 33.44 }, { true, 16.72 }, { true, 50.161 },
    { false, 0 }, { true, 33.538 }, { true, 33.526 }, { true, 33.447 },
    { true, 33.445 }, { true, 33.441 }, { true, 16.721 }, { true, 33.44 },
    { true, 33.44 }, { true, 50.161 }, { true, 16.72 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { false, 0 }, { false, 16.72 },
    { true, 66.881 }, { true, 16.72 }, { false, 16.72 }, { true, 50.16 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { true, 50.161 }, { false, 0 },
    { true, 33.44 }, { true, 33.44 }, { true, 50.161 }, { true, 16.72 },
    { true, 33.44 }, { true, 33.441 }, { false, 0 }, { true, 66.88 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { false, 0 },
    { true, 33.441 }, { true, 33.44 }, { true, 33.44 }, { false, 0 },
    { true, 16.72 }, { true, 50.161 }, { false, 0 }, { true, 50.16 },
    { false, 0.001 }, { true, 16.721 }, { true, 66.88 }, { true, 33.44 },
    { true, 33.441 }, { true, 33.44 }, { true, 50.161 }, { true, 16.72 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 66.881 },
    { true, 33.44 }, { true, 16.72 }, { true, 33.441 }, { false, 16.72 },
    { true, 66.88 }, { true, 16.721 }, { true, 50.16 }, { true, 33.44 },
    { true, 16.72 }, { true, 33.441 }, { true, 33.44 }, { true, 33.44 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

TEST(SmoothEventSamplerTest, DrawingAt60FpsWith60HzVsyncSampledAt30Hertz) {
  // Actual capturing of timing data: WebGL Acquarium demo
  // (http://webglsamples.googlecode.com/hg/aquarium/aquarium.html) which ran
  // between 55-60 FPS in the steady-state.
  static const DataPoint data_points[] = {
    { true, 16.72 }, { true, 16.72 }, { true, 4163.29 }, { true, 50.193 },
    { true, 117.041 }, { true, 50.161 }, { true, 50.16 }, { true, 33.441 },
    { true, 50.16 }, { true, 33.44 }, { false, 0 }, { false, 0 },
    { true, 50.161 }, { true, 83.601 }, { true, 50.16 }, { true, 16.72 },
    { true, 33.441 }, { false, 16.72 }, { true, 50.16 }, { true, 16.72 },
    { false, 0.001 }, { true, 33.441 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 33.44 }, { false, 16.721 }, { true, 16.721 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 16.721 }, { true, 50.161 }, { false, 0 },
    { true, 33.44 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.721 }, { true, 33.44 },
    { false, 0 }, { true, 33.44 }, { false, 16.721 }, { true, 16.721 },
    { true, 50.161 }, { false, 0 }, { true, 16.72 }, { true, 33.44 },
    { false, 0 }, { true, 33.441 }, { false, 16.72 }, { true, 16.72 },
    { true, 50.16 }, { false, 0 }, { true, 16.72 }, { true, 33.441 },
    { true, 33.44 }, { false, 0 }, { true, 33.44 }, { true, 33.441 },
    { false, 0 }, { true, 33.44 }, { true, 33.441 }, { false, 0 },
    { true, 33.44 }, { false, 0 }, { true, 33.44 }, { false, 16.72 },
    { true, 16.721 }, { true, 50.161 }, { false, 0 }, { true, 16.72 },
    { true, 33.44 }, { true, 33.441 }, { false, 0 }, { true, 33.44 },
    { true, 33.44 }, { false, 0 }, { true, 33.441 }, { false, 16.72 },
    { true, 16.72 }, { true, 50.16 }, { false, 0 }, { true, 16.72 },
    { true, 33.441 }, { false, 0 }, { true, 33.44 }, { false, 16.72 },
    { true, 33.44 }, { false, 0 }, { true, 16.721 }, { true, 50.161 },
    { false, 0 }, { true, 16.72 }, { true, 33.44 }, { false, 0 },
    { true, 33.441 }, { false, 16.72 }, { true, 16.72 }, { true, 50.16 }
  };

  SmoothEventSampler sampler(base::TimeDelta::FromSeconds(1) / 30, true, 3);
  ReplayCheckingSamplerDecisions(data_points, arraysize(data_points), &sampler);
}

}  // namespace
}  // namespace content
