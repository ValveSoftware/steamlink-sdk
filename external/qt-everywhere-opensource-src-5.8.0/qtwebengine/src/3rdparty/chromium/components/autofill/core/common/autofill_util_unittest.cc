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

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "suggestion = " << kTestCases[i].field_suggestion
                 << ", contents = " << kTestCases[i].field_contents
                 << ", case_sensitive = " << kTestCases[i].case_sensitive);

    EXPECT_EQ(kTestCases[i].expected_result,
              FieldIsSuggestionSubstringStartingOnTokenBoundary(
                  base::ASCIIToUTF16(kTestCases[i].field_suggestion),
                  base::ASCIIToUTF16(kTestCases[i].field_contents),
                  kTestCases[i].case_sensitive));
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

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "suggestion = " << kTestCases[i].field_suggestion
                 << ", contents = " << kTestCases[i].field_contents
                 << ", case_sensitive = " << kTestCases[i].case_sensitive);

    EXPECT_EQ(kTestCases[i].expected_start,
              GetTextSelectionStart(
                  base::ASCIIToUTF16(kTestCases[i].field_suggestion),
                  base::ASCIIToUTF16(kTestCases[i].field_contents),
                  kTestCases[i].case_sensitive));
  }
}

}  // namespace autofill
