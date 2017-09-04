// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests for FieldIsSuggestionSubstringStartingOnTokenBoundary().
TEST(AutofillUtilTest, FieldIsSuggestionSubstringStartingOnTokenBoundary) {
  // FieldIsSuggestionSubstringStartingOnTokenBoundary should not work yet
  // without a flag.
  EXPECT_FALSE(FieldIsSuggestionSubstringStartingOnTokenBoundary(
      base::ASCIIToUTF16("ab@cd.b"), base::ASCIIToUTF16("b"), false));

  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  const struct {
    const char* const field_suggestion;
    const char* const field_contents;
    bool case_sensitive;
    bool expected_result;
  } kTestCases[] = {
      {"ab@cd.b", "a",    false, true},
      {"ab@cd.b", "b",    false, true},
      {"ab@cd.b", "Ab",   false, true},
      {"ab@cd.b", "Ab",   true,  false},
      {"ab@cd.b", "cd",   true,  true},
      {"ab@cd.b", "d",    false, false},
      {"ab@cd.b", "b@",   true,  false},
      {"ab@cd.b", "ab",   false, true},
      {"ab@cd.b", "cd.b", true,  true},
      {"ab@cd.b", "b@cd", false, false},
      {"ab@cd.b", "ab@c", false, true},
      {"ba.a.ab", "a.a",  false, true},
      {"",        "ab",   false, false},
      {"",        "ab",   true,  false},
      {"ab",      "",     false, true},
      {"ab",      "",     true,  true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "suggestion = " << test_case.field_suggestion
                 << ", contents = " << test_case.field_contents
                 << ", case_sensitive = " << test_case.case_sensitive);

    EXPECT_EQ(test_case.expected_result,
              FieldIsSuggestionSubstringStartingOnTokenBoundary(
                  base::ASCIIToUTF16(test_case.field_suggestion),
                  base::ASCIIToUTF16(test_case.field_contents),
                  test_case.case_sensitive));
  }
}

// Tests for GetTextSelectionStart().
TEST(AutofillUtilTest, GetTextSelectionStart) {
  const size_t kInvalid = base::string16::npos;
  const struct {
    const char* const field_suggestion;
    const char* const field_contents;
    bool case_sensitive;
    size_t expected_start;
  } kTestCases[] = {
      {"ab@cd.b",              "a",       false, 1},
      {"ab@cd.b",              "A",       true,  kInvalid},
      {"ab@cd.b",              "Ab",      false, 2},
      {"ab@cd.b",              "Ab",      true,  kInvalid},
      {"ab@cd.b",              "cd",      false, 5},
      {"ab@cd.b",              "ab@c",    false, 4},
      {"ab@cd.b",              "cd.b",    false, 7},
      {"ab@cd.b",              "b@cd",    false, kInvalid},
      {"ab@cd.b",              "b",       false, 7},
      {"ba.a.ab",              "a.a",     false, 6},
      {"texample@example.com", "example", false, 16},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "suggestion = " << test_case.field_suggestion
                 << ", contents = " << test_case.field_contents
                 << ", case_sensitive = " << test_case.case_sensitive);

    EXPECT_EQ(
        test_case.expected_start,
        GetTextSelectionStart(base::ASCIIToUTF16(test_case.field_suggestion),
                              base::ASCIIToUTF16(test_case.field_contents),
                              test_case.case_sensitive));
  }
}

// Tests for LowercaseAndTokenizeAttributeString
TEST(AutofillUtilTest, LowercaseAndTokenizeAttributeString) {
  const struct {
    const char* const attribute;
    std::vector<std::string> tokens;
  } kTestCases[] = {
      // Test leading and trailing whitespace, test tabs and newlines
      {"foo bar baz", {"foo", "bar", "baz"}},
      {" foo bar baz ", {"foo", "bar", "baz"}},
      {"foo\tbar baz ", {"foo", "bar", "baz"}},
      {"foo\nbar baz ", {"foo", "bar", "baz"}},

      // Test different forms of capitalization
      {"FOO BAR BAZ", {"foo", "bar", "baz"}},
      {"foO baR bAz", {"foo", "bar", "baz"}},

      // Test collapsing of multiple whitespace characters in a row
      {"  \t\t\n\n   ", std::vector<std::string>()},
      {"foO    baR bAz", {"foo", "bar", "baz"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "attribute = " << test_case.attribute);

    EXPECT_EQ(test_case.tokens,
              LowercaseAndTokenizeAttributeString(test_case.attribute));
  }
}
}  // namespace autofill
