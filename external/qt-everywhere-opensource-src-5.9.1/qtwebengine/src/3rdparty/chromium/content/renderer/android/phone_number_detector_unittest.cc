// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/phone_number_detector.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PhoneNumberDetectorTest : public testing::Test {
 public:
  static std::string FindNumber(const std::string& content,
                                const std::string& region) {
    base::string16 content_16 = base::UTF8ToUTF16(content);
    base::string16 result_16;
    size_t start, end;
    PhoneNumberDetector detector(region);
    std::string content_text;
    if (detector.FindContent(content_16.begin(), content_16.end(),
                             &start, &end, &content_text))
         result_16 = content_16.substr(start, end - start);
    return base::UTF16ToUTF8(result_16);
  }

  static std::string FindAndFormatNumber(const std::string& content,
                                         const std::string& region) {
    base::string16 content_16 = base::UTF8ToUTF16(content);
    base::string16 result_16;
    size_t start, end;
    PhoneNumberDetector detector(region);
    std::string content_text;
    detector.FindContent(content_16.begin(), content_16.end(),
                         &start, &end, &content_text);
    return content_text;
  }
};

TEST_F(PhoneNumberDetectorTest, FindNumber) {
  // Tests cases with valid home numbers.
  EXPECT_EQ("617-426-3000", FindNumber("hello 617-426-3000 blah", "us"));
  EXPECT_EQ("", FindNumber("hello 617-426-3000 blah", "gb"));
  EXPECT_EQ("020-7617-4426", FindNumber("<div>020-7617-4426</div>", "gb"));
  EXPECT_EQ("", FindNumber("<div>020-7617-4426</div>", "fr"));
  EXPECT_EQ("02.38.96.68.88", FindNumber("Tel:02.38.96.68.88", "fr"));
  EXPECT_EQ("", FindNumber("Tel:02.38.96.68.88", "gb"));
  EXPECT_EQ("1-800-866-2453",
            FindNumber("You can call this number:1-800-866-2453 for more "
                       "information", "us"));
  EXPECT_EQ("+1 203-925-4602", FindNumber("+1 203-925-4602", "us"));
}

TEST_F(PhoneNumberDetectorTest, FindAndFormatNumber) {
  EXPECT_EQ("+16174263000",
            FindAndFormatNumber("hello 617-426-3000 blah", "us"));
  EXPECT_EQ("", FindAndFormatNumber("hello 617-426-3000 blah", "gb"));
  EXPECT_EQ("02076174426",
            FindAndFormatNumber("<div>020-7617-4426</div>", "gb"));
  EXPECT_EQ("", FindAndFormatNumber("<div>020-7617-4426</div>", "fr"));
  EXPECT_EQ("0238966888", FindAndFormatNumber("Tel:02.38.96.68.88", "fr"));
  EXPECT_EQ("+18008662453",
            FindAndFormatNumber("You can call this number:1-800-866-2453 for"
                                "more information", "us"));
  EXPECT_EQ("+12039254602", FindAndFormatNumber("+1 203-925-4602", "us"));

  // "+1 (650) 333-6000" using fullwidth UTF-8 characters.
  EXPECT_EQ("+16503336000", FindAndFormatNumber(
            "\xEF\xBC\x8B\xEF\xBC\x91\xE3\x80\x80\xEF\xBC\x88"
            "\xEF\xBC\x96\xEF\xBC\x95\xEF\xBC\x90\xEF\xBC\x89"
            "\xE3\x80\x80\xEF\xBC\x93\xEF\xBC\x93\xEF\xBC\x93"
            "\xE3\x83\xBC\xEF\xBC\x96\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90", "us"));
}

TEST_F(PhoneNumberDetectorTest, FindAndFormatAlphaNumber) {
  // Tests cases with valid alpha numbers.
  EXPECT_EQ("+18002378289", FindAndFormatNumber("1 800-BestBuy", "us"));
  EXPECT_EQ("+18002378289", FindAndFormatNumber("1-800-BEST-BUY", "us"));
  EXPECT_EQ("+18002378289", FindAndFormatNumber("1-800-BEST BUY", "us"));
  // TODO(qinmin): support alpha number detection when there are characters on
  // either side of the string.
  EXPECT_EQ("", FindAndFormatNumber("hello 1-800-BEST-BUY", "us"));
  EXPECT_EQ("", FindAndFormatNumber("BestBuy", "us"));
  EXPECT_EQ("", FindAndFormatNumber("1-BestBuy", "us"));
  EXPECT_EQ("", FindAndFormatNumber("1 BestBuy", "us"));
}

}  // namespace content
