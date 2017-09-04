// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/GenericFontFamilySettings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GenericFontFamilySettingsTest, FirstAvailableFontFamily) {
  GenericFontFamilySettings settings;
  EXPECT_TRUE(settings.standard().isEmpty());

  // Returns the first available font if starts with ",".
  settings.updateStandard(",not exist, Arial");
  EXPECT_EQ("Arial", settings.standard());

  // Otherwise returns any strings as they were set.
  AtomicString nonLists[] = {
      "Arial", "not exist", "not exist, Arial",
  };
  for (const AtomicString& name : nonLists) {
    settings.updateStandard(name);
    EXPECT_EQ(name, settings.standard());
  }
}

}  // namespace blink
