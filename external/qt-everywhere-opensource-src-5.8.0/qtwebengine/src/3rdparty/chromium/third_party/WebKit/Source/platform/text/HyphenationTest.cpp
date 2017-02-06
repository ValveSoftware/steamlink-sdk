// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "platform/Logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NoHyphenation : public Hyphenation {
public:
    size_t lastHyphenLocation(const StringView&, size_t beforeIndex) const override
    {
        return 0;
    }
};

TEST(HyphenationTest, GetHyphenation)
{
    RefPtr<Hyphenation> hyphenation = adoptRef(new NoHyphenation);
    Hyphenation::setForTesting("en-US", hyphenation);
    EXPECT_EQ(hyphenation.get(), Hyphenation::get("en-US"));

    Hyphenation::setForTesting("en-UK", nullptr);
    EXPECT_EQ(nullptr, Hyphenation::get("en-UK"));

    Hyphenation::clearForTesting();
}

TEST(HyphenationTest, GetHyphenationCaseFolding)
{
    RefPtr<Hyphenation> hyphenation = adoptRef(new NoHyphenation);
    Hyphenation::setForTesting("en-US", hyphenation);
    EXPECT_EQ(hyphenation, Hyphenation::get("en-US"));
    EXPECT_EQ(hyphenation, Hyphenation::get("en-us"));

    Hyphenation::clearForTesting();
}

} // namespace blink
