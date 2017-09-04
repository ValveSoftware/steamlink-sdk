// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/url_pattern_matching.h"

#include <vector>

#include "components/subresource_filter/core/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

constexpr proto::AnchorType kAnchorNone = proto::ANCHOR_TYPE_NONE;
constexpr proto::AnchorType kBoundary = proto::ANCHOR_TYPE_BOUNDARY;
constexpr proto::AnchorType kSubdomain = proto::ANCHOR_TYPE_SUBDOMAIN;

}  // namespace

TEST(UrlPatternMatchingTest, BuildFailureFunctionForUrlPattern) {
  const struct {
    UrlPattern url_pattern;
    std::vector<size_t> expected_failure_function;
  } kTestCases[] = {
      {{"abcd", proto::URL_PATTERN_TYPE_SUBSTRING}, {0, 0, 0, 0}},
      {{"&a?a/"}, {0, 0, 0, 0, 0}},
      {{"^a?a/"}, {1, 0, 0, 1, 2, 3}},

      {{"abc*abc", kBoundary, kAnchorNone}, {0, 0, 0}},
      {{"abc*aaa", kBoundary, kAnchorNone}, {0, 1, 2}},
      {{"aaa*abc", kBoundary, kAnchorNone}, {0, 0, 0}},

      {{"abc*abc", kAnchorNone, kBoundary}, {0, 0, 0}},
      {{"abc*aaa", kAnchorNone, kBoundary}, {0, 0, 0}},
      {{"aaa*abc", kAnchorNone, kBoundary}, {0, 1, 2}},

      {{"abc*cca", kSubdomain, kAnchorNone}, {0, 0, 0, 0, 1, 0}},
      {{"abc*cca", kBoundary, kAnchorNone}, {0, 1, 0}},
      {{"abc*cca"}, {0, 0, 0, 0, 1, 0}},

      {{"abc*abacaba*cab"}, {0, 0, 0, 0, 0, 1, 0, 1, 2, 3, 0, 0, 0}},
      {{"aaa*a^d*^^b"}, {0, 1, 2, 1, 0, 0, 0, 1, 0, 1, 0}},
      {{"aaa*a^d*^^b", kAnchorNone, kBoundary}, {0, 1, 2, 1, 0, 0, 0}},
      {{"^^a*a^d*^^b", kBoundary, kAnchorNone}, {1, 0, 0, 0, 1, 0, 1, 0}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Pattern: " << test_case.url_pattern.url_pattern
                 << "; Anchors: "
                 << static_cast<int>(test_case.url_pattern.anchor_left) << ", "
                 << static_cast<int>(test_case.url_pattern.anchor_right));

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.url_pattern, &failure);
    EXPECT_EQ(test_case.expected_failure_function, failure);
  }
}

TEST(UrlPatternMatchingTest, IsUrlPatternMatch) {
  const struct {
    UrlPattern url_pattern;
    const char* url;
    bool expect_match;
  } kTestCases[] = {
      {{"xampl", proto::URL_PATTERN_TYPE_SUBSTRING},
       "http://example.com",
       true},
      {{"example", proto::URL_PATTERN_TYPE_SUBSTRING},
       "http://example.com",
       true},
      {{"/a?a"}, "http://ex.com/a?a", true},
      {{"^abc"}, "http://ex.com/abc?a", true},
      {{"^abc"}, "http://ex.com/a?abc", true},
      {{"^abc"}, "http://ex.com/abc?abc", true},

      {{"http://ex", kBoundary, kAnchorNone}, "http://example.com", true},
      {{"mple.com/", kAnchorNone, kBoundary}, "http://example.com", true},

      // Note: "example.com" will be normalized into "example.com/".
      {{"http://*mpl", kBoundary, kAnchorNone}, "http://example.com", true},
      {{"mpl*com/", kAnchorNone, kBoundary}, "http://example.com", true},
      {{"example^com"}, "http://example.com", false},
      {{"example^com"}, "http://example/com", true},
      {{"example.com^"}, "http://example.com:8080", true},
      {{"http*.com/", kBoundary, kBoundary}, "http://example.com", true},
      {{"http*.org/", kBoundary, kBoundary}, "http://example.com", false},

      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path/bbb?k=v&p1=0&p2=1", false},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&p1=0&p2=1", true},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&k=v&p1=0&p2=1", true},
      {{"/path?*&p1=*&p2="},
       "http://ex.com/aaa/path?k=v&p1=0&p3=10&p2=1",
       true},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path&p1=0&p2=1", false},
      {{"/path?*&p1=*&p2="}, "http://ex.com/aaa/path?k=v&p2=0&p1=1", false},

      {{"abc*def*ghijk*xyz"},
       "http://example.com/abcdeffffghijkmmmxyzzz",
       true},
      {{"abc*cdef"}, "http://example.com/abcdef", false},

      {{"^^a^^"}, "http://ex.com/?a=/", true},
      {{"^^a^^"}, "http://ex.com/?a=/&b=0", true},
      {{"^^a^^"}, "http://ex.com/?a=", false},

      {{"ex.com^path^*k=v^"}, "http://ex.com/path/?k1=v1&ak=v&kk=vv", true},
      {{"ex.com^path^*k=v^"}, "http://ex.com/p/path/?k1=v1&ak=v&kk=vv", false},
      {{"a^a&a^a&"}, "http://ex.com/a/a/a/a/?a&a&a&a&a", true},

      {{"abc*def^"}, "http://ex.com/abc/a/ddef/", true},

      {{"https://example.com/"}, "http://example.com/", false},
      {{"example.com/", kSubdomain, kAnchorNone}, "http://example.com/", true},
      {{"examp", kSubdomain, kAnchorNone}, "http://example.com/", true},
      {{"xamp", kSubdomain, kAnchorNone}, "http://example.com/", false},
      {{"examp", kSubdomain, kAnchorNone}, "http://test.example.com/", true},
      {{"t.examp", kSubdomain, kAnchorNone}, "http://test.example.com/", false},
      {{"com^", kSubdomain, kAnchorNone}, "http://test.example.com/", true},
      {{"x.com", kSubdomain, kAnchorNone}, "http://ex.com/?url=x.com", false},
      {{"ex.com/", kSubdomain, kBoundary}, "http://ex.com/", true},
      {{"ex.com^", kSubdomain, kBoundary}, "http://ex.com/", true},
      {{"ex.co", kSubdomain, kBoundary}, "http://ex.com/", false},
      {{"ex.com", kSubdomain, kBoundary}, "http://rex.com.ex.com/", false},
      {{"ex.com/", kSubdomain, kBoundary}, "http://rex.com.ex.com/", true},
      {{"http", kSubdomain, kBoundary}, "http://http.com/", false},
      {{"http", kSubdomain, kAnchorNone}, "http://http.com/", true},
      {{"/example.com", kSubdomain, kBoundary}, "http://example.com/", false},
      {{"/example.com/", kSubdomain, kBoundary}, "http://example.com/", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern.url_pattern
                 << "; Anchors: "
                 << static_cast<int>(test_case.url_pattern.anchor_left) << ", "
                 << static_cast<int>(test_case.url_pattern.anchor_right)
                 << "; URL: " << GURL(test_case.url));

    std::vector<size_t> failure;
    BuildFailureFunction(test_case.url_pattern, &failure);
    const bool is_match =
        IsUrlPatternMatch(GURL(test_case.url), test_case.url_pattern,
                          failure.begin(), failure.end());
    EXPECT_EQ(test_case.expect_match, is_match);
  }
}

}  // namespace subresource_filter
