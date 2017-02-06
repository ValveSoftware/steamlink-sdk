/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/Font.h"

#include "platform/fonts/TypesettingFeatures.h"
#include "platform/testing/FontTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::testing::createTestFont;

namespace blink {

static inline String fontPath(String relativePath)
{
    return testing::blinkRootDir()
        + "/Source/platform/testing/data/"
        + relativePath;
}

TEST(FontPlatformDataTest, AhemHasNoSpaceInLigaturesOrKerning)
{
    Font font = createTestFont("Ahem", fontPath("Ahem.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();
    TypesettingFeatures features = Kerning | Ligatures;

    EXPECT_FALSE(platformData.hasSpaceInLigaturesOrKerning(features));
}

TEST(FontPlatformDataTest, AhemSpaceLigatureHasSpaceInLigaturesOrKerning)
{
    Font font = createTestFont("AhemSpaceLigature", fontPath("AhemSpaceLigature.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();
    TypesettingFeatures features = Kerning | Ligatures;

    EXPECT_TRUE(platformData.hasSpaceInLigaturesOrKerning(features));
}

TEST(FontPlatformDataTest, AhemSpaceLigatureHasNoSpaceWithoutFontFeatures)
{
    Font font = createTestFont("AhemSpaceLigature", fontPath("AhemSpaceLigature.woff"), 16);
    const FontPlatformData& platformData = font.primaryFont()->platformData();
    TypesettingFeatures features = 0;

    EXPECT_FALSE(platformData.hasSpaceInLigaturesOrKerning(features));
}

} // namespace blink
