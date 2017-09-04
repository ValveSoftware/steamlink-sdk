// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;

namespace content {

TEST(SiteIsolationStatsGathererTest, SniffForJS) {
  StringPiece basic_js_data("var a = 4");
  StringPiece js_data("\t\t\r\n var a = 4");
  StringPiece json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  StringPiece empty_data("");

  EXPECT_TRUE(SiteIsolationStatsGatherer::SniffForJS(js_data));
  EXPECT_FALSE(SiteIsolationStatsGatherer::SniffForJS(json_data));

  // Basic bounds check.
  EXPECT_FALSE(SiteIsolationStatsGatherer::SniffForJS(empty_data));
}

}  // namespace content
