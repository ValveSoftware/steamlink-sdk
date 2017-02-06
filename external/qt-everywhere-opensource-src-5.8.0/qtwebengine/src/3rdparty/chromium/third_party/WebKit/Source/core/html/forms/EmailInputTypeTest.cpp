// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/EmailInputType.h"

#include "bindings/core/v8/ScriptRegexp.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void expectToSucceed(const String& source)
{
    std::unique_ptr<ScriptRegexp> emailRegexp = EmailInputType::createEmailRegexp();
    String result = EmailInputType::convertEmailAddressToASCII(*emailRegexp, source);
    EXPECT_NE(source, result);
    EXPECT_TRUE(EmailInputType::isValidEmailAddress(*emailRegexp, result));
}

void expectToFail(const String& source)
{
    std::unique_ptr<ScriptRegexp> emailRegexp = EmailInputType::createEmailRegexp();
    // Conversion failed.  The resultant value might contains non-ASCII
    // characters, and not a valid email address.
    EXPECT_FALSE(EmailInputType::isValidEmailAddress(*emailRegexp, EmailInputType::convertEmailAddressToASCII(*emailRegexp, source)));
}

} // namespace

TEST(EmailInputTypeTest, ConvertEmailAddressToASCII)
{
    // U+043C U+043E U+0439 . U+0434 U+043E U+043C U+0435 U+043D
    expectToFail(String::fromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9.\xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD@"));
    expectToFail(String::fromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9. \xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD"));
    expectToFail(String::fromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9.\t\xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD"));
}

TEST(EmailInputTypeTest, ConvertEmailAddressToASCIIUTS46)
{
    // http://unicode.org/reports/tr46/#Table_IDNA_Comparisons

    // U+00E0
    expectToSucceed(String::fromUTF8("foo@\xC3\xA0.com"));
    // U+FF01
    expectToFail(String::fromUTF8("foo@\xEF\xBC\x81.com"));

    // U+2132
    expectToFail(String::fromUTF8("foo@\xE2\x84\xB2.com"));
    // U+2F868
    expectToFail(String::fromUTF8("foo@\xF0\xAF\xA1\xA8.com"));

    // U+00C0
    expectToSucceed(String::fromUTF8("foo@\xC3\x80.com"));
    // U+2665
    expectToSucceed(String::fromUTF8("foo@\xE2\x99\xA5.com"));
    // U+00DF
    expectToSucceed(String::fromUTF8("foo@\xC3\x9F.com"));

    // U+0221
    expectToSucceed(String::fromUTF8("foo@\xC8\xA1.com"));
    // U+0662
    expectToFail(String::fromUTF8("foo@\xD8\x82.com"));

    // U+2615
    expectToSucceed(String::fromUTF8("foo@\xE2\x98\x95.com"));
    // U+023A
    expectToSucceed(String::fromUTF8("foo@\xC8\xBA.com"));
}

} // namespace blink
