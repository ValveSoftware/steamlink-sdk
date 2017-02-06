// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/large_icon_url_parser.h"

#include <stddef.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestUrlStr[] = "https://www.google.ca/imghp?hl=en&tab=wi";

}  // namespace

TEST(LargeIconUrlParserTest, ParseLargeIconPathSuccess) {
  // Everything populated.
  {
    LargeIconUrlParser parser;
    EXPECT_TRUE(parser.Parse(std::string("48/") + kTestUrlStr));
    EXPECT_EQ(48, parser.size_in_pixels());
    EXPECT_EQ(GURL(kTestUrlStr), GURL(parser.url_string()));
    EXPECT_EQ(3U, parser.path_index());
  }

  // Empty URL.
  {
    LargeIconUrlParser parser;
    EXPECT_TRUE(parser.Parse("48/"));
    EXPECT_EQ(48, parser.size_in_pixels());
    EXPECT_EQ(GURL(), GURL(parser.url_string()));
    EXPECT_EQ(3U, parser.path_index());
  }

  // Tolerate invalid URL.
  {
    LargeIconUrlParser parser;
    EXPECT_TRUE(parser.Parse("48/NOT A VALID URL"));
    EXPECT_EQ(48, parser.size_in_pixels());
    EXPECT_EQ("NOT A VALID URL", parser.url_string());
    EXPECT_EQ(3U, parser.path_index());
  }
}

TEST(LargeIconUrlParserTest, ParseLargeIconPathFailure) {
  const char* const test_cases[] = {
    "",
    "/",
    "//",
    "48",  // Missing '/'.
    "/http://www.google.com/",  // Missing size.
    "not_a_number/http://www.google.com/",  // Bad size.
    "0/http://www.google.com/",  // Non-positive size.
    "-1/http://www.google.com/",  // Non-positive size.
  };
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    LargeIconUrlParser parser;
    EXPECT_FALSE(parser.Parse(test_cases[i])) << "test_cases[" << i << "]";
  }
}
