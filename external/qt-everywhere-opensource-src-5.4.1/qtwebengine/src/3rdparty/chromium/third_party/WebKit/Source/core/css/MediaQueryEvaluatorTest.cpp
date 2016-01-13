// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaQueryEvaluator.h"

#include "core/css/MediaList.h"
#include "core/css/MediaValuesCached.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"

#include <gtest/gtest.h>

namespace WebCore {

typedef struct {
    const char* input;
    const bool output;
} TestCase;

TEST(MediaQueryEvaluatorTest, Basic)
{
    // The first string represents the input string.
    // The second string represents the output string, if present.
    // Otherwise, the output string is identical to the first string.
    TestCase screenTestCases[] = {
        {"", 1},
        {" ", 1},
        {"screen", 1},
        {"screen and (color)", 1},
        {"all and (min-width: 500px)", 1},
        {"not screen and (color)", 0},
        {"(min-width: 500px)", 1},
        {"(min-width: 501px)", 0},
        {"(max-width: 500px)", 1},
        {"(max-width: 499px)", 0},
        {"(width: 500px)", 1},
        {"(width: 501px)", 0},
        {"(min-height: 500px)", 1},
        {"(min-height: 501px)", 0},
        {"(max-height: 500px)", 1},
        {"(max-height: 499px)", 0},
        {"(height: 500px)", 1},
        {"(height: 501px)", 0},
        {"screen and (min-width: 400px) and (max-width: 700px)", 1},
        {"screen and (device-aspect-ratio: 16/9)", 0},
        {"screen and (device-aspect-ratio: 1/1)", 1},
        {"all and (min-color: 2)", 1},
        {"all and (min-color: 32)", 0},
        {"all and (min-color-index: 0)", 1},
        {"all and (min-color-index: 1)", 0},
        {"all and (monochrome)", 0},
        {"all and (min-monochrome: 0)", 1},
        {"all and (grid: 0)", 1},
        {"(resolution: 2dppx)", 1},
        {"(resolution: 1dppx)", 0},
        {"(orientation: portrait)", 1},
        {"(orientation: landscape)", 0},
        {"tv and (scan: progressive)", 0},
        {"(pointer: coarse)", 0},
        {"(pointer: fine)", 1},
        {"(hover: 1)", 1},
        {"(hover: 0)", 0},
        {0, 0} // Do not remove the terminator line.
    };
    TestCase printTestCases[] = {
        {"print and (min-resolution: 1dppx)", 1},
        {"print and (min-resolution: 118dpcm)", 0},
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

    for (unsigned i = 0; screenTestCases[i].input; ++i) {
        RefPtrWillBeRawPtr<MediaQuerySet> querySet = MediaQuerySet::create(screenTestCases[i].input);
        MediaQueryEvaluator mediaQueryEvaluator("screen", *mediaValues);
        ASSERT_EQ(screenTestCases[i].output, mediaQueryEvaluator.eval(querySet.get()));
    }
    for (unsigned i = 0; printTestCases[i].input; ++i) {
        RefPtrWillBeRawPtr<MediaQuerySet> querySet = MediaQuerySet::create(printTestCases[i].input);
        MediaQueryEvaluator mediaQueryEvaluator("print", *mediaValues);
        ASSERT_EQ(printTestCases[i].output, mediaQueryEvaluator.eval(querySet.get()));
    }
}

} // namespace
