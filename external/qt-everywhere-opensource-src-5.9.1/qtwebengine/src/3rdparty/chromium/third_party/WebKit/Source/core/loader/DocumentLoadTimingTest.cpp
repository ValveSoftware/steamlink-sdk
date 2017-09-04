// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/DocumentLoadTiming.h"

#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class DocumentLoadTimingTest : public testing::Test {};

TEST_F(DocumentLoadTimingTest, ensureValidNavigationStartAfterEmbedder) {
  std::unique_ptr<DummyPageHolder> dummyPage = DummyPageHolder::create();
  DocumentLoadTiming timing(*(dummyPage->document().loader()));

  double delta = -1000;
  double embedderNavigationStart = monotonicallyIncreasingTime() + delta;
  timing.setNavigationStart(embedderNavigationStart);

  double realWallTime = currentTime();
  double adjustedWallTime =
      timing.monotonicTimeToPseudoWallTime(timing.navigationStart());

  EXPECT_NEAR(adjustedWallTime, realWallTime + delta, .001);
}

TEST_F(DocumentLoadTimingTest, correctTimingDeltas) {
  std::unique_ptr<DummyPageHolder> dummyPage = DummyPageHolder::create();
  DocumentLoadTiming timing(*(dummyPage->document().loader()));

  double navigationStartDelta = -456;
  double currentMonotonicTime = monotonicallyIncreasingTime();
  double embedderNavigationStart = currentMonotonicTime + navigationStartDelta;

  timing.setNavigationStart(embedderNavigationStart);

  // Super quick load! Expect the wall time reported by this event to be
  // dominated by the navigationStartDelta, but similar to currentTime().
  timing.markLoadEventEnd();
  double realWallLoadEventEnd = currentTime();
  double adjustedLoadEventEnd =
      timing.monotonicTimeToPseudoWallTime(timing.loadEventEnd());

  EXPECT_NEAR(adjustedLoadEventEnd, realWallLoadEventEnd, .001);

  double adjustedNavigationStart =
      timing.monotonicTimeToPseudoWallTime(timing.navigationStart());
  EXPECT_NEAR(adjustedLoadEventEnd - adjustedNavigationStart,
              -navigationStartDelta, .001);
}

}  // namespace blink
