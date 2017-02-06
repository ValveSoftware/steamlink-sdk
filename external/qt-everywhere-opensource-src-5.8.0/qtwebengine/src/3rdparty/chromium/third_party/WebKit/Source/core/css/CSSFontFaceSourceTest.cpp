// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontFaceSource.h"

#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DummyFontFaceSource : public CSSFontFaceSource {
public:
    PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override
    {
        return SimpleFontData::create(FontPlatformData(fromSkSp(SkTypeface::MakeDefault()), "", 0, false, false));
    }

    DummyFontFaceSource() { }

    PassRefPtr<SimpleFontData> getFontDataForSize(float size)
    {
        FontDescription fontDescription;
        fontDescription.setSizeAdjust(size);
        fontDescription.setAdjustedSize(size);
        return getFontData(fontDescription);
    }
};

namespace {

unsigned simulateHashCalculation(float size)
{
    FontDescription fontDescription;
    fontDescription.setSizeAdjust(size);
    fontDescription.setAdjustedSize(size);
    return fontDescription.cacheKey(FontFaceCreationParams()).hash();
}

}

TEST(CSSFontFaceSourceTest, HashCollision)
{
    DummyFontFaceSource fontFaceSource;
    // Even if the hash value collide, fontface cache should return different value for different fonts.
    EXPECT_EQ(simulateHashCalculation(2821), simulateHashCalculation(3346));
    EXPECT_NE(fontFaceSource.getFontDataForSize(2821), fontFaceSource.getFontDataForSize(3346));
}

} // namespace blink
