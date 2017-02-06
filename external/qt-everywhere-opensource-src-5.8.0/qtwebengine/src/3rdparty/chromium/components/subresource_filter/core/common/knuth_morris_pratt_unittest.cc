// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/knuth_morris_pratt.h"

#include <cctype>
#include <functional>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

bool NumerophobicEquivalentTo(char first, char second) {
  return first == second || (std::isdigit(first) && std::isdigit(second));
}

template <typename EquivalentTo = std::equal_to<char>>
std::vector<size_t> FindAllOccurrences(
    base::StringPiece string,
    base::StringPiece pattern,
    const std::vector<size_t>& failure_function,
    EquivalentTo equivalent_to = EquivalentTo()) {
  std::vector<size_t> positions;

  size_t position = 0;
  size_t relative_position = FindFirstOccurrence(
      string, pattern, failure_function.data(), equivalent_to);

  while (relative_position != base::StringPiece::npos) {
    position += relative_position;
    positions.push_back(position);
    relative_position =
        FindNextOccurrence(string.substr(position), pattern,
                           failure_function.data(), equivalent_to);
  }

  return positions;
}

}  // namespace

TEST(KnuthMorrisPrattTest, BuildFailureFunctionRegular) {
  const struct {
    std::string string;
    std::vector<size_t> expected_failure_function;
  } kTestCases[] = {
      {"", std::vector<size_t>()},
      {"a", {0}},
      {"aa", {0, 1}},
      {"aaa", {0, 1, 2}},
      {"abc", {0, 0, 0}},
      {"abca", {0, 0, 0, 1}},
      {"abacaba", {0, 0, 1, 0, 1, 2, 3}},
      {"str-sstrr", {0, 0, 0, 0, 1, 1, 2, 3, 0}},
      {"sts-ststs", {0, 0, 1, 0, 1, 2, 3, 2, 3}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "String: " << test_case.string);

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.string, &failure);
    EXPECT_EQ(test_case.expected_failure_function, failure);
  }
}

TEST(KnuthMorrisPrattTest, BuildFailureFunctionWithEquivalentChars) {
  const struct {
    std::string string;
    std::vector<size_t> expected_failure_function;
  } kTestCases[] = {
      {"123", {0, 1, 2}},
      {"0str1str", {0, 0, 0, 0, 1, 2, 3, 4}},
      {"7str8srr", {0, 0, 0, 0, 1, 2, 0, 0}},
      {"567str9sr", {0, 1, 2, 0, 0, 0, 1, 0, 0}},
      {"567str999sr", {0, 1, 2, 0, 0, 0, 1, 2, 3, 4, 0}},
      {"str9sr6", {0, 0, 0, 0, 1, 0, 0}},
      {"5a6a7a8a9a", {0, 0, 1, 2, 3, 4, 5, 6, 7, 8}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "String: " << test_case.string);

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.string, &failure, NumerophobicEquivalentTo);
    EXPECT_EQ(test_case.expected_failure_function, failure);
  }
}

TEST(KnuthMorrisPrattTest, FindSubstringsRegular) {
  const struct {
    std::string string;
    std::string pattern;
    std::vector<size_t> expected_positions;
  } kTestCases[] = {
      {"", "abc", std::vector<size_t>()},
      {"abc", "", {0, 1, 2, 3}},
      {"abc", "abc", {3}},
      {"aaaabc", "abc", {6}},
      {"abcccc", "abc", {3}},
      {"aabcc", "abc", {4}},
      {"aabcabcabcc", "abc", {4, 7, 10}},
      {"abcabcabc", "abc", {3, 6, 9}},
      {"abab", "abc", std::vector<size_t>()},
      {"ababac", "abc", std::vector<size_t>()},
      {"aaaaaa", "a", {1, 2, 3, 4, 5, 6}},
      {"aaaaaa", "aaa", {3, 4, 5, 6}},
      {"abababa", "aba", {3, 5, 7}},
      {"acdacdacdac", "cdacd", {6, 9}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "String: " << test_case.string
                                    << "; Pattern: " << test_case.pattern);

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.pattern, &failure);

    std::vector<size_t> positions =
        FindAllOccurrences(test_case.string, test_case.pattern, failure);
    EXPECT_EQ(test_case.expected_positions, positions);

    auto occurrences = CreateAllOccurrences(test_case.string, test_case.pattern,
                                            failure.data());
    positions.assign(occurrences.begin(), occurrences.end());
    EXPECT_EQ(test_case.expected_positions, positions);
  }
}

TEST(KnuthMorrisPrattTest, FindSubstringsWithEquivalentChars) {
  const struct {
    std::string string;
    std::string pattern;
    std::vector<size_t> expected_positions;
  } kTestCases[] = {
      {"", "abc", std::vector<size_t>()},
      {"012", "1", {1, 2, 3}},
      {"012", "012", {3}},
      {"123456", "000", {3, 4, 5, 6}},
      {"0a1a2a3a", "1a2", {3, 5, 7}},
      {"0aa1a2aa3a", "1aa2", {4, 9}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "String: " << test_case.string
                                    << "; Pattern: " << test_case.pattern);

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.pattern, &failure, NumerophobicEquivalentTo);

    std::vector<size_t> positions = FindAllOccurrences(
        test_case.string, test_case.pattern, failure, NumerophobicEquivalentTo);
    EXPECT_EQ(test_case.expected_positions, positions);

    auto occurrences =
        CreateAllOccurrences(test_case.string, test_case.pattern,
                             failure.data(), NumerophobicEquivalentTo);
    positions.assign(occurrences.begin(), occurrences.end());
    EXPECT_EQ(test_case.expected_positions, positions);
  }
}

TEST(KnuthMorrisPrattTest, AllOccurrencesIterator) {
  const struct {
    std::string string;
    std::string pattern;
    std::vector<size_t> expected_positions;
  } kTestCases[] = {
      {"", "abc", std::vector<size_t>()},
      {"abc", "", {0, 1, 2, 3}},
      {"abc", "abc", {3}},
      {"aaa", "a", {1, 2, 3}},
      {"aaaabc", "abc", {6}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "String: " << test_case.string
                                    << "; Pattern: " << test_case.pattern);

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.pattern, &failure);

    auto occurrences = CreateAllOccurrences(test_case.string, test_case.pattern,
                                            failure.data());

    auto iterator = occurrences.begin();
    const size_t size = test_case.expected_positions.size();
    for (size_t i = 0; i != size; ++i, ++iterator) {
      const size_t expected_position = test_case.expected_positions[i];
      EXPECT_EQ(expected_position, *iterator);

      auto iterator_copy = iterator;
      EXPECT_EQ(iterator, iterator_copy);
      EXPECT_EQ(expected_position, *iterator_copy);

      EXPECT_EQ(iterator_copy, iterator++);
      EXPECT_NE(iterator_copy, iterator);
      if (i + 1 != size) {
        EXPECT_NE(occurrences.end(), iterator);
        EXPECT_EQ(test_case.expected_positions[i + 1], *iterator);
      } else {
        EXPECT_EQ(occurrences.end(), iterator);
      }

      iterator = iterator_copy;
      EXPECT_EQ(expected_position, *iterator);
    }

    EXPECT_EQ(occurrences.end(), iterator);
  }
}

}  // namespace subresource_filter
