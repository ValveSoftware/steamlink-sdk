// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippet.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace {

TEST(NTPSnippetTest, FromChromeContentSuggestionsDictionary) {
  const std::string kJsonStr =
      "{"
      "  \"id\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"summaryText\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"publishTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"publisherName\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "}";
  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  ASSERT_TRUE(json_value->GetAsDictionary(&json_dict));

  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json_dict);
  ASSERT_THAT(snippet, testing::NotNull());

  EXPECT_EQ(snippet->id(), "http://localhost/foobar");
  EXPECT_EQ(snippet->title(), "Foo Barred from Baz");
  EXPECT_EQ(snippet->snippet(), "...");
  EXPECT_EQ(snippet->salient_image_url(), GURL("http://localhost/foobar.jpg"));
  auto unix_publish_date = snippet->publish_date() - base::Time::UnixEpoch();
  auto expiry_duration = snippet->expiry_date() - snippet->publish_date();
  EXPECT_FLOAT_EQ(unix_publish_date.InSecondsF(), 1467284497.000000f);
  EXPECT_FLOAT_EQ(expiry_duration.InSecondsF(), 86400.000000f);

  EXPECT_EQ(snippet->best_source().publisher_name, "Foo News");
  EXPECT_EQ(snippet->best_source().url, GURL("http://localhost/foobar"));
  EXPECT_EQ(snippet->best_source().amp_url, GURL("http://localhost/amp"));
}

}  // namespace
}  // namespace ntp_snippets
