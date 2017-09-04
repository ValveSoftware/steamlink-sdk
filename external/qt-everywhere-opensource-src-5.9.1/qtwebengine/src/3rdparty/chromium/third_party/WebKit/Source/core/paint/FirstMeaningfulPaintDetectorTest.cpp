// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/paint/PaintTiming.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FirstMeaningfulPaintDetectorTest : public testing::Test {
 protected:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    s_timeElapsed = 0.0;
    m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
  }

  void TearDown() override {
    setTimeFunctionsForTesting(m_originalTimeFunction);
  }

  Document& document() { return m_dummyPageHolder->document(); }
  PaintTiming& paintTiming() { return PaintTiming::from(document()); }
  FirstMeaningfulPaintDetector& detector() {
    return paintTiming().firstMeaningfulPaintDetector();
  }

  void simulateLayoutAndPaint(int newElements) {
    StringBuilder builder;
    for (int i = 0; i < newElements; i++)
      builder.append("<span>a</span>");
    document().write(builder.toString());
    document().updateStyleAndLayout();
    detector().notifyPaint();
  }

  void simulateNetworkStable() { detector().networkStableTimerFired(nullptr); }

 private:
  static double returnMockTime() {
    s_timeElapsed += 1.0;
    return s_timeElapsed;
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  TimeFunction m_originalTimeFunction;
  static double s_timeElapsed;
};

double FirstMeaningfulPaintDetectorTest::s_timeElapsed;

TEST_F(FirstMeaningfulPaintDetectorTest, NoFirstPaint) {
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
}

TEST_F(FirstMeaningfulPaintDetectorTest, OneLayout) {
  paintTiming().markFirstPaint();
  simulateLayoutAndPaint(1);
  double afterPaint = monotonicallyIncreasingTime();
  EXPECT_EQ(paintTiming().firstMeaningfulPaint(), 0.0);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterPaint);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantSecond) {
  paintTiming().markFirstPaint();
  simulateLayoutAndPaint(1);
  double afterLayout1 = monotonicallyIncreasingTime();
  simulateLayoutAndPaint(10);
  double afterLayout2 = monotonicallyIncreasingTime();
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), afterLayout1);
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout2);
}

TEST_F(FirstMeaningfulPaintDetectorTest, TwoLayoutsSignificantFirst) {
  paintTiming().markFirstPaint();
  simulateLayoutAndPaint(10);
  double afterLayout1 = monotonicallyIncreasingTime();
  simulateLayoutAndPaint(1);
  simulateNetworkStable();
  EXPECT_GT(paintTiming().firstMeaningfulPaint(), paintTiming().firstPaint());
  EXPECT_LT(paintTiming().firstMeaningfulPaint(), afterLayout1);
}

}  // namespace blink
