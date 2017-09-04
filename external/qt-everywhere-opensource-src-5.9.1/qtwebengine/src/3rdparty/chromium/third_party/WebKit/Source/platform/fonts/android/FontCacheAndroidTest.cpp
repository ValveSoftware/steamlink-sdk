// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCache.h"

#include "platform/fonts/SimpleFontData.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontCacheAndroid, fallbackFontForCharacter) {
  // A Latin character in the common locale system font, but not in the
  // Chinese locale-preferred font.
  const UChar32 testChar = 228;

  FontDescription fontDescription;
  fontDescription.setLocale(LayoutLocale::get("zh"));
  ASSERT_EQ(USCRIPT_SIMPLIFIED_HAN, fontDescription.script());
  fontDescription.setGenericFamily(FontDescription::StandardFamily);

  FontCache* fontCache = FontCache::fontCache();
  ASSERT_TRUE(fontCache);
  RefPtr<SimpleFontData> fontData =
      fontCache->fallbackFontForCharacter(fontDescription, testChar, 0);
  EXPECT_TRUE(fontData);
}

TEST(FontCacheAndroid, genericFamilyNameForScript) {
  FontDescription english;
  english.setLocale(LayoutLocale::get("en"));
  FontDescription chinese;
  chinese.setLocale(LayoutLocale::get("zh"));

  if (FontFamilyNames::webkit_standard.isEmpty())
    FontFamilyNames::init();

  // For non-CJK, getGenericFamilyNameForScript should return the given
  // familyName.
  EXPECT_EQ(FontFamilyNames::webkit_standard,
            FontCache::getGenericFamilyNameForScript(
                FontFamilyNames::webkit_standard, english));
  EXPECT_EQ(FontFamilyNames::webkit_monospace,
            FontCache::getGenericFamilyNameForScript(
                FontFamilyNames::webkit_monospace, english));

  // For CJK, getGenericFamilyNameForScript should return CJK fonts except
  // monospace.
  EXPECT_NE(FontFamilyNames::webkit_standard,
            FontCache::getGenericFamilyNameForScript(
                FontFamilyNames::webkit_standard, chinese));
  EXPECT_EQ(FontFamilyNames::webkit_monospace,
            FontCache::getGenericFamilyNameForScript(
                FontFamilyNames::webkit_monospace, chinese));
}

}  // namespace blink
