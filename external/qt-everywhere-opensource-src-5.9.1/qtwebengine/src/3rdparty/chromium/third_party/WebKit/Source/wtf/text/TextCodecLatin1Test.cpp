// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextCodecLatin1.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextCodec.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/TextEncodingRegistry.h"
#include <memory>

TEST(TextCodecLatin1Test, QuestionMarksAndSurrogates) {
  WTF::TextEncoding encoding("windows-1252");
  std::unique_ptr<WTF::TextCodec> codec(newTextCodec(encoding));

  {
    const LChar testCase[] = {0xd1, 0x16, 0x86};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->encode(testCase, testCaseSize,
                                   WTF::QuestionMarksForUnencodables);
    EXPECT_STREQ("\xd1\x16?", result.data());
  }
  {
    const UChar testCase[] = {0xd9f0, 0xdcd9};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->encode(testCase, testCaseSize,
                                   WTF::QuestionMarksForUnencodables);
    EXPECT_STREQ("?", result.data());
  }
  {
    const UChar testCase[] = {0xd9f0, 0xdcd9, 0xd9f0, 0xdcd9};
    size_t testCaseSize = WTF_ARRAY_LENGTH(testCase);
    CString result = codec->encode(testCase, testCaseSize,
                                   WTF::QuestionMarksForUnencodables);
    EXPECT_STREQ("??", result.data());
  }
}
