/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontDescription.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {


static inline void assertDescriptionMatchesMask(FontDescription& source, FontTraitsBitfield bitfield)
{
    FontDescription target;
    target.setTraits(FontTraits(bitfield));
    EXPECT_EQ(source.style(), target.style());
    EXPECT_EQ(source.weight(), target.weight());
    EXPECT_EQ(source.stretch(), target.stretch());
}

TEST(FontDescriptionTest, TestFontTraits)
{
    FontDescription source;
    source.setStyle(FontStyleNormal);
    source.setWeight(FontWeightNormal);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleNormal);
    source.setWeight(FontWeightNormal);
    source.setStretch(FontStretchExtraCondensed);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight900);
    source.setStretch(FontStretchUltraExpanded);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight100);
    source.setStretch(FontStretchExtraExpanded);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight900);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight800);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight700);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight600);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight500);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight400);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight300);
    source.setStretch(FontStretchUltraExpanded);
    assertDescriptionMatchesMask(source, source.traits().bitfield());

    source.setStyle(FontStyleItalic);
    source.setWeight(FontWeight200);
    source.setStretch(FontStretchNormal);
    assertDescriptionMatchesMask(source, source.traits().bitfield());
}

TEST(FontDescriptionTest, TestHashCollision)
{
    FontWeight weights[] = {
        FontWeight100,
        FontWeight200,
        FontWeight300,
        FontWeight400,
        FontWeight500,
        FontWeight600,
        FontWeight700,
        FontWeight800,
        FontWeight900,
    };
    FontStretch stretches[] {
        FontStretchUltraCondensed,
        FontStretchExtraCondensed,
        FontStretchCondensed,
        FontStretchSemiCondensed,
        FontStretchNormal,
        FontStretchSemiExpanded,
        FontStretchExpanded,
        FontStretchExtraExpanded,
        FontStretchUltraExpanded
    };
    FontStyle styles[] = {
        FontStyleNormal,
        FontStyleOblique,
        FontStyleItalic
    };

    FontDescription source;
    WTF::Vector<unsigned> hashes;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(weights); i++) {
        source.setWeight(weights[i]);
        for (size_t j = 0; j < WTF_ARRAY_LENGTH(stretches); j++) {
            source.setStretch(stretches[j]);
            for (size_t k = 0; k < WTF_ARRAY_LENGTH(styles); k++) {
                source.setStyle(styles[k]);
                unsigned hash = source.styleHashWithoutFamilyList();
                ASSERT_FALSE(hashes.contains(hash));
                hashes.append(hash);
            }
        }
    }

}

} // namespace blink
