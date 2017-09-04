// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_regexes.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

TEST(AutofillRegexesTest, SampleRegexes) {
  struct TestCase {
    const char* const input;
    const char* const pattern;
  };

  const TestCase kPositiveCases[] = {
    // Empty pattern
    {"", ""},
    {"Look, ma' -- a non-empty string!", ""},
    // Substring
    {"string", "tri"},
    // Substring at beginning
    {"string", "str"},
    {"string", "^str"},
    // Substring at end
    {"string", "ring"},
    {"string", "ring$"},
    // Case-insensitive
    {"StRiNg", "string"},
  };
  for (const auto& test_case : kPositiveCases) {
    SCOPED_TRACE(test_case.input);
    SCOPED_TRACE(test_case.pattern);
    EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input),
                               ASCIIToUTF16(test_case.pattern)));
  }

  const TestCase kNegativeCases[] = {
    // Empty string
    {"", "Look, ma' -- a non-empty pattern!"},
    // Substring
    {"string", "trn"},
    // Substring at beginning
    {"string", " str"},
    {"string", "^tri"},
    // Substring at end
    {"string", "ring "},
    {"string", "rin$"},
  };
  for (const auto& test_case : kNegativeCases) {
    SCOPED_TRACE(test_case.input);
    SCOPED_TRACE(test_case.pattern);
    EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input),
                                ASCIIToUTF16(test_case.pattern)));
  }
}

TEST(AutofillRegexesTest, ExpirationDate2DigitYearRegexes) {
  struct TestCase {
    const char* const input;
  };

  const base::string16 pattern = ASCIIToUTF16(kExpirationDate2DigitYearRe);

  const TestCase kPositiveCases[] = {
    // Simple two year cases
    {"mm / yy"},
    {"mm/ yy"},
    {"mm /yy"},
    {"mm/yy"},
    {"mm - yy"},
    {"mm- yy"},
    {"mm -yy"},
    {"mm-yy"},
    {"mmyy"},
    // Complex two year cases
    {"Expiration Date (MM / YY)"},
    {"Expiration Date (MM/YY)"},
    {"Expiration Date (MM - YY)"},
    {"Expiration Date (MM-YY)"},
    {"Expiration Date MM / YY"},
    {"Expiration Date MM/YY"},
    {"Expiration Date MM - YY"},
    {"Expiration Date MM-YY"},
    {"expiration date yy"},
    {"Exp Date     (MM / YY)"},
  };

  for (const auto& test_case : kPositiveCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input),pattern));
  }

  const TestCase kNegativeCases[] = {
    {""},
    {"Look, ma' -- an invalid string!"},
    {"mmfavouritewordyy"},
    {"mm a yy"},
    {"mm a yyyy"},
    // Simple four year cases
    {"mm / yyyy"},
    {"mm/ yyyy"},
    {"mm /yyyy"},
    {"mm/yyyy"},
    {"mm - yyyy"},
    {"mm- yyyy"},
    {"mm -yyyy"},
    {"mm-yyyy"},
    {"mmyyyy"},
    // Complex four year cases
    {"Expiration Date (MM / YYYY)"},
    {"Expiration Date (MM/YYYY)"},
    {"Expiration Date (MM - YYYY)"},
    {"Expiration Date (MM-YYYY)"},
    {"Expiration Date MM / YYYY"},
    {"Expiration Date MM/YYYY"},
    {"Expiration Date MM - YYYY"},
    {"Expiration Date MM-YYYY"},
    {"expiration date yyyy"},
    {"Exp Date     (MM / YYYY)"},
  };

  for (const auto& test_case : kNegativeCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
  }
}

TEST(AutofillRegexesTest, ExpirationDate4DigitYearRegexes) {
  struct TestCase {
    const char* const input;
  };

  const base::string16 pattern = ASCIIToUTF16(kExpirationDate4DigitYearRe);

  const TestCase kPositiveCases[] = {
    // Simple four year cases
    {"mm / yyyy"},
    {"mm/ yyyy"},
    {"mm /yyyy"},
    {"mm/yyyy"},
    {"mm - yyyy"},
    {"mm- yyyy"},
    {"mm -yyyy"},
    {"mm-yyyy"},
    {"mmyyyy"},
    // Complex four year cases
    {"Expiration Date (MM / YYYY)"},
    {"Expiration Date (MM/YYYY)"},
    {"Expiration Date (MM - YYYY)"},
    {"Expiration Date (MM-YYYY)"},
    {"Expiration Date MM / YYYY"},
    {"Expiration Date MM/YYYY"},
    {"Expiration Date MM - YYYY"},
    {"Expiration Date MM-YYYY"},
    {"expiration date yyyy"},
    {"Exp Date     (MM / YYYY)"},
  };

  for (const auto& test_case : kPositiveCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input),pattern));
  }

  const TestCase kNegativeCases[] = {
    {""},
    {"Look, ma' -- an invalid string!"},
    {"mmfavouritewordyy"},
    {"mm a yy"},
    {"mm a yyyy"},
    // Simple two year cases
    {"mm / yy"},
    {"mm/ yy"},
    {"mm /yy"},
    {"mm/yy"},
    {"mm - yy"},
    {"mm- yy"},
    {"mm -yy"},
    {"mm-yy"},
    {"mmyy"},
    // Complex two year cases
    {"Expiration Date (MM / YY)"},
    {"Expiration Date (MM/YY)"},
    {"Expiration Date (MM - YY)"},
    {"Expiration Date (MM-YY)"},
    {"Expiration Date MM / YY"},
    {"Expiration Date MM/YY"},
    {"Expiration Date MM - YY"},
    {"Expiration Date MM-YY"},
    {"expiration date yy"},
    {"Exp Date     (MM / YY)"},
  };

  for (const auto& test_case : kNegativeCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
  }
}

}  // namespace autofill
