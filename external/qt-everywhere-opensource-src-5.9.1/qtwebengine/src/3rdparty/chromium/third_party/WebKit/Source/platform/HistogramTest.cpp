// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Histogram.h"

#include "base/metrics/histogram_samples.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ScopedUsHistogramTimerTest : public ::testing::Test {
 public:
  static void advanceClock(double microseconds) {
    s_timeElapsed += microseconds;
  }

 protected:
  static double returnMockTime() { return s_timeElapsed; }

  virtual void SetUp() {
    s_timeElapsed = 0.0;
    m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
  }

  virtual void TearDown() {
    setTimeFunctionsForTesting(m_originalTimeFunction);
  }

 private:
  static double s_timeElapsed;
  TimeFunction m_originalTimeFunction;
};

double ScopedUsHistogramTimerTest::s_timeElapsed;

class TestCustomCountHistogram : public CustomCountHistogram {
 public:
  TestCustomCountHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucketCount)
      : CustomCountHistogram(name, min, max, bucketCount) {}

  base::HistogramBase* histogram() { return m_histogram; }
};

TEST_F(ScopedUsHistogramTimerTest, Basic) {
  TestCustomCountHistogram scopedUsCounter("test", 0, 10000000, 50);
  {
    ScopedUsHistogramTimer timer(scopedUsCounter);
    advanceClock(0.5);
  }
  // 0.5s == 500000us
  EXPECT_EQ(500000, scopedUsCounter.histogram()->SnapshotSamples()->sum());
}

}  // namespace blink
