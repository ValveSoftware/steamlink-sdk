// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "platform/fonts/mac/FontFamilyMatcherMac.h"

#include <AppKit/AppKit.h>
#include <gtest/gtest.h>

#import "platform/mac/VersionUtilMac.h"

@interface NSString (YosemiteAdditions)
- (BOOL)containsString:(NSString*)string;
@end

namespace blink {

void TestSystemFontContainsString(FontWeight desiredWeight, NSString* substring)
{
    NSFont* font = MatchNSFontFamily(@"BlinkMacSystemFont", 0, desiredWeight, 11);
    EXPECT_TRUE([font.description containsString:substring]);
}

TEST(FontFamilyMatcherMacTest, YosemiteFontWeights)
{
    if (!IsOSYosemite())
        return;

    TestSystemFontContainsString(FontWeight100, @"-UltraLight");
    TestSystemFontContainsString(FontWeight200, @"-Thin");
    TestSystemFontContainsString(FontWeight300, @"-Light");
    TestSystemFontContainsString(FontWeight400, @"-Regular");
    TestSystemFontContainsString(FontWeight500, @"-Medium");
    TestSystemFontContainsString(FontWeight600, @"-Bold");
    TestSystemFontContainsString(FontWeight700, @"-Bold");
    TestSystemFontContainsString(FontWeight800, @"-Heavy");
    TestSystemFontContainsString(FontWeight900, @"-Heavy");
}

} // namespace blink
