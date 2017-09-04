// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "core/page/WindowFeatures.h"

#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>

namespace blink {

using WindowFeaturesTest = ::testing::Test;

TEST_F(WindowFeaturesTest, NoOpener) {
  static const struct {
    const char* featureString;
    bool noopener;
  } cases[] = {
      {"", false},
      {"something", false},
      {"something, something", false},
      {"notnoopener", false},
      {"noopener", true},
      {"something, noopener", true},
      {"noopener, something", true},
      {"NoOpEnEr", true},
  };

  for (const auto& test : cases) {
    WindowFeatures features(test.featureString);
    EXPECT_EQ(test.noopener, features.noopener) << "Testing '"
                                                << test.featureString << "'";
  }
}

}  // namespace blink
