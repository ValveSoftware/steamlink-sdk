// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "platform/LayoutLocale.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NoHyphenation : public Hyphenation {
 public:
  size_t lastHyphenLocation(const StringView&,
                            size_t beforeIndex) const override {
    return 0;
  }
};

TEST(HyphenationTest, Get) {
  RefPtr<Hyphenation> hyphenation = adoptRef(new NoHyphenation);
  LayoutLocale::setHyphenationForTesting("en-US", hyphenation);
  EXPECT_EQ(hyphenation.get(), LayoutLocale::get("en-US")->getHyphenation());

  LayoutLocale::setHyphenationForTesting("en-UK", nullptr);
  EXPECT_EQ(nullptr, LayoutLocale::get("en-UK")->getHyphenation());

  LayoutLocale::clearForTesting();
}

}  // namespace blink
