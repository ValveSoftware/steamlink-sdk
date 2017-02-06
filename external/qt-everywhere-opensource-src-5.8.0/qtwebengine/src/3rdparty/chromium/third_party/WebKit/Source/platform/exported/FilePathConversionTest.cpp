// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/FilePathConversion.h"

#include "base/files/file_path.h"
#include "public/platform/WebString.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(FilePathConversionTest, convert)
{
    String test8bitString("path");
    String test8bitLatin1("a\xC4");

    static const UChar test[5] = { 0x0070, 0x0061, 0x0074, 0x0068, 0 }; // path
    static const UChar testLatin1[3] = { 0x0061, 0x00C4, 0 }; // a\xC4
    static const UChar testUTF16[3] = { 0x6587, 0x5B57, 0 }; // \u6587 \u5B57
    String test16bitString(test);
    String test16bitLatin1(testLatin1);
    String test16bitUTF16(testUTF16);

    // Latin1 a\xC4 == UTF8 a\xC3\x84
    base::FilePath pathLatin1 = base::FilePath::FromUTF8Unsafe("a\xC3\x84");
    // UTF16 \u6587\u5B57 == \xE6\x96\x87\xE5\xAD\x97
    base::FilePath pathUTF16 = base::FilePath::FromUTF8Unsafe("\xE6\x96\x87\xE5\xAD\x97");

    EXPECT_TRUE(test8bitString.is8Bit());
    EXPECT_TRUE(test8bitLatin1.is8Bit());
    EXPECT_FALSE(test16bitString.is8Bit());
    EXPECT_FALSE(test16bitLatin1.is8Bit());

    EXPECT_EQ(FILE_PATH_LITERAL("path"), WebStringToFilePath(test8bitString).value());
    EXPECT_EQ(pathLatin1.value(), WebStringToFilePath(test8bitLatin1).value());
    EXPECT_EQ(FILE_PATH_LITERAL("path"), WebStringToFilePath(test16bitString).value());
    EXPECT_EQ(pathLatin1.value(), WebStringToFilePath(test16bitLatin1).value());
    EXPECT_EQ(pathUTF16.value(), WebStringToFilePath(test16bitUTF16).value());
}

} // namespace blink
