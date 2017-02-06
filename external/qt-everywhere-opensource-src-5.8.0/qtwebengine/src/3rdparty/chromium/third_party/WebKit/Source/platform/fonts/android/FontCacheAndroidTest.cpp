// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCache.h"

#include "platform/fonts/SimpleFontData.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontCacheAndroid, fallbackFontForCharacter)
{
    // A Latin character in the common locale system font, but not in the
    // Chinese locale-preferred font.
    const UChar32 testChar = 228;

    FontDescription fontDescription;
    fontDescription.setLocale("zh");
    ASSERT_EQ(USCRIPT_SIMPLIFIED_HAN, fontDescription.script());
    fontDescription.setGenericFamily(FontDescription::StandardFamily);

    FontCache* fontCache = FontCache::fontCache();
    ASSERT_TRUE(fontCache);
    RefPtr<SimpleFontData> fontData = fontCache->fallbackFontForCharacter(fontDescription, testChar, 0);
    EXPECT_TRUE(fontData);
}

} // namespace blink
