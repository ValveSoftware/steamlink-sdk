// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "media/base/scoped_histogram_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ScopedHistogramTimer, TwoTimersOneScope) {
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer0");
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer1");
}

}  // namespace media
