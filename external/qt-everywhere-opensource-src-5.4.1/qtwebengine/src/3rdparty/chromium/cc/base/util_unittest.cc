// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(UtilTest, RoundUp) {
  for (int multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in descending order, so that we can
    // determine the correct value before it's needed.
    int correct;
    for (int attempt = 5 * multiplier; attempt >= -5 * multiplier; --attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, RoundUp(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }

  for (unsigned multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in descending order, so that we can
    // determine the correct value before it's needed.
    unsigned correct;
    for (unsigned attempt = 5 * multiplier; attempt > 0; --attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, RoundUp(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
    EXPECT_EQ(0u, RoundUp(0u, multiplier))
        << "attempt=0 multiplier=" << multiplier;
  }
}

TEST(UtilTest, RoundDown) {
  for (int multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in ascending order, so that we can
    // determine the correct value before it's needed.
    int correct;
    for (int attempt = -5 * multiplier; attempt <= 5 * multiplier; ++attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, RoundDown(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }

  for (unsigned multiplier = 1; multiplier <= 10; ++multiplier) {
    // Try attempts in ascending order, so that we can
    // determine the correct value before it's needed.
    unsigned correct;
    for (unsigned attempt = 0; attempt <= 5 * multiplier; ++attempt) {
      if ((attempt % multiplier) == 0)
        correct = attempt;
      EXPECT_EQ(correct, RoundDown(attempt, multiplier))
          << "attempt=" << attempt << " multiplier=" << multiplier;
    }
  }
}

}  // namespace
}  // namespace cc
