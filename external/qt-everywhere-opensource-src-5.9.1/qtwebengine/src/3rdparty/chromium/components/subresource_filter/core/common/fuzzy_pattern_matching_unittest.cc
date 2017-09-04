// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/fuzzy_pattern_matching.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(FuzzyPatternMatchingTest, StartsWithFuzzy) {
  const struct {
    const char* text;
    const char* subpattern;
    bool expected_starts_with;
  } kTestCases[] = {
      {"abc", "", true},       {"abc", "a", true},     {"abc", "ab", true},
      {"abc", "abc", true},    {"abc", "abcd", false}, {"abc", "abc^^", false},
      {"abc", "abcd^", false}, {"abc", "ab^", false},  {"abc", "bc", false},
      {"abc", "bc^", false},   {"abc", "^abc", false},
  };
  // TODO(pkalinnikov): Make end-of-string match '^' again.

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    const bool starts_with =
        StartsWithFuzzy(test_case.text, test_case.subpattern);
    EXPECT_EQ(test_case.expected_starts_with, starts_with);
  }
}

TEST(FuzzyPatternMatchingTest, EndsWithFuzzy) {
  const struct {
    const char* text;
    const char* subpattern;
    bool expected_ends_with;
  } kTestCases[] = {
      {"abc", "", true},       {"abc", "c", true},     {"abc", "bc", true},
      {"abc", "abc", true},    {"abc", "0abc", false}, {"abc", "abc^^", false},
      {"abc", "abcd^", false}, {"abc", "ab^", false},  {"abc", "ab", false},
      {"abc", "^abc", false},
  };
  // TODO(pkalinnikov): Make end-of-string match '^' again.

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    const bool ends_with = EndsWithFuzzy(test_case.text, test_case.subpattern);
    EXPECT_EQ(test_case.expected_ends_with, ends_with);
  }
}
TEST(FuzzyPatternMatchingTest, AllOccurrences) {
  const struct {
    const char* text;
    const char* subpattern;
    std::vector<size_t> expected_matches;
  } kTestCases[] = {
      {"abcd", "", {0, 1, 2, 3, 4}},
      {"abcd", "de", std::vector<size_t>()},
      {"abcd", "ab", {2}},
      {"abcd", "bc", {3}},
      {"abcd", "cd", {4}},

      {"a/bc/a/b", "", {0, 1, 2, 3, 4, 5, 6, 7, 8}},
      {"a/bc/a/b", "de", std::vector<size_t>()},
      {"a/bc/a/b", "a/", {2, 7}},
      {"a/bc/a/c", "a/c", {8}},
      {"a/bc/a/c", "a^c", {8}},
      {"a/bc/a/c", "a?c", std::vector<size_t>()},

      {"ab^cd", "ab/cd", std::vector<size_t>()},
      {"ab^cd", "b/c", std::vector<size_t>()},
      {"ab^cd", "ab^cd", {5}},
      {"ab^cd", "b^c", {4}},
      {"ab^b/b", "b/b", {6}},

      {"a/a/a/a", "a/a", {3, 5, 7}},
      {"a/a/a/a", "^a^a^a", {7}},
      {"a/a/a/a", "^a^a?a", std::vector<size_t>()},
      {"a/a/a/a", "?a?a?a", std::vector<size_t>()},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    std::vector<size_t> failure;
    BuildFailureFunctionFuzzy(test_case.subpattern, &failure);

    const auto& occurrences = AllOccurrencesFuzzy<size_t>(
        test_case.text, test_case.subpattern, failure.data());
    std::vector<size_t> matches(occurrences.begin(), occurrences.end());
    EXPECT_EQ(test_case.expected_matches, matches);
  }
}

}  // namespace subresource_filter
