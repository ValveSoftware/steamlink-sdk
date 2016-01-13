// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "printing/printing_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

std::string Simplify(const char* title) {
  return base::UTF16ToUTF8(SimplifyDocumentTitle(base::ASCIIToUTF16(title)));
}

TEST(PrintingUtilsTest, SimplifyDocumentTitle) {
  EXPECT_STREQ("", Simplify("").c_str());
  EXPECT_STREQ("Long string. Long string...ng string. Long string.",
               Simplify("Long string. Long string. Long string. Long string. "
                        "Long string. Long string. Long string.").c_str());
  EXPECT_STREQ("Control Characters",
               Simplify("C\ron\ntrol Charac\15ters").c_str());
  EXPECT_STREQ("", Simplify("\n\r\n\r\t\r").c_str());
}

}  // namespace printing

