// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/email_detector.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class EmailDetectorTest : public testing::Test {
 public:
  static void FindAndCheckEmail(const std::string& content,
                                const std::string& expected) {
    base::string16 content_16 = base::UTF8ToUTF16(content);
    base::string16 result_16;
    size_t start, end;
    EmailDetector detector;
    std::string content_text;
    if (detector.FindContent(content_16.begin(), content_16.end(),
                             &start, &end, &content_text)) {
      result_16 = content_16.substr(start, end - start);
    }
    EXPECT_EQ(expected, base::UTF16ToUTF8(result_16));
    EXPECT_EQ(expected, content_text);
  }
};

TEST_F(EmailDetectorTest, FindEmail) {
  FindAndCheckEmail("please email test@testing.com", "test@testing.com");
  FindAndCheckEmail("please email test@123.456.co.uk.", "test@123.456.co.uk");
  FindAndCheckEmail("My email is 'a@b.org'.", "a@b.org");
  FindAndCheckEmail("123@bcd.org", "123@bcd.org");
  FindAndCheckEmail("[quitelongwelllongemailaddress@somequitelongdomain.org]",
                    "quitelongwelllongemailaddress@somequitelongdomain.org");
  FindAndCheckEmail("Should find the first@email.org not the second@email.org",
                    "first@email.org");
  FindAndCheckEmail("Email:HELLO@SOMETHING.COM", "HELLO@SOMETHING.COM");
  FindAndCheckEmail("Email SOMEONE@GOOGLE.COM for details.",
                    "SOMEONE@GOOGLE.COM");
  FindAndCheckEmail("It's \"testadd@company.fr\"", "testadd@company.fr");
  FindAndCheckEmail("This is not an @emailaddress.com", "");
  FindAndCheckEmail("Apples @2.50 each", "");
  FindAndCheckEmail("Log on to google.com", "");
  FindAndCheckEmail("Try someone@, they might know.", "");
  FindAndCheckEmail("No, bob@com is not an email address.", "");
  FindAndCheckEmail("@", "");
  FindAndCheckEmail("Just bob @google.com", "");
  FindAndCheckEmail("Why not call larry@google and ask him.", "");
}

}  // namespace content
