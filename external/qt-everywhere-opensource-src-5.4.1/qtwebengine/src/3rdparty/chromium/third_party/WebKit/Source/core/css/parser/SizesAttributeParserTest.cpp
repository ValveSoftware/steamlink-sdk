// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/SizesAttributeParser.h"

#include "core/css/MediaValuesCached.h"

#include <gtest/gtest.h>

namespace WebCore {

typedef struct {
    const char* input;
    const int output;
} TestCase;

TEST(SizesAttributeParserTest, Basic)
{
    TestCase testCases[] = {
        {"screen", 500},
        {"(min-width:500px)", 500},
        {"(min-width:500px) 200px", 200},
        {"(min-width:500px) 50vw", 250},
        {"(min-width:500px) 200px, 400px", 200},
        {"400px, (min-width:500px) 200px", 400},
        {"(min-width:5000px) 200px, 400px", 400},
        {"(blalbadfsdf) 200px, 400px", 400},
        {"0", 0},
        {"-0", 0},
        {"1", 500},
        {"300px, 400px", 300},
        {"(min-width:5000px) 200px, (min-width:500px) 400px", 400},
        {"", 500},
        {"  ", 500},
        {" /**/ ", 500},
        {" /**/ 300px", 300},
        {"300px /**/ ", 300},
        {" /**/ (min-width:500px) /**/ 300px", 300},
        {"-100px, 200px", 200},
        {"-50vw, 20vw", 100},
        {"50asdf, 200px", 200},
        {"asdf, 200px", 200},
        {"(max-width: 3000px) 200w, 400w", 500},
        {",, , /**/ ,200px", 200},
        {"50vw", 250},
        {"calc(40vw*2)", 400},
        {"(min-width:5000px) calc(5000px/10), (min-width:500px) calc(1200px/3)", 400},
        {"(min-width:500px) calc(1200/3)", 500},
        {"(min-width:500px) calc(1200px/(0px*14))", 500},
        {"(max-width: 3000px) 200px, 400px", 200},
        {"(max-width: 3000px) 20em, 40em", 320},
        {"(max-width: 3000px) 0, 40em", 0},
        {"(max-width: 3000px) 50vw, 40em", 250},
        {0, 0} // Do not remove the terminator line.
    };

    MediaValuesCached::MediaValuesCachedData data;
    data.viewportWidth = 500;
    data.viewportHeight = 500;
    data.deviceWidth = 500;
    data.deviceHeight = 500;
    data.devicePixelRatio = 2.0;
    data.colorBitsPerComponent = 24;
    data.monochromeBitsPerComponent = 0;
    data.pointer = MediaValues::MousePointer;
    data.defaultFontSize = 16;
    data.threeDEnabled = true;
    data.scanMediaType = false;
    data.screenMediaType = true;
    data.printMediaType = false;
    data.strictMode = true;
    RefPtr<MediaValues> mediaValues = MediaValuesCached::create(data);

    for (unsigned i = 0; testCases[i].input; ++i) {
        int effectiveSize = SizesAttributeParser::findEffectiveSize(testCases[i].input, mediaValues);
        ASSERT_EQ(testCases[i].output, effectiveSize);
    }
}

} // namespace
