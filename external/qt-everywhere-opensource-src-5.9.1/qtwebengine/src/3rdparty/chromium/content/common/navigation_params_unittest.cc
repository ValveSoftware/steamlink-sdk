// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "content/public/common/browser_side_navigation_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(NavigationParamsTest, ShouldMakeNetworkRequestForURL) {
  if (!IsBrowserSideNavigationEnabled())
    return;

  EXPECT_TRUE(ShouldMakeNetworkRequestForURL(GURL("http://foo/bar.html")));
  EXPECT_TRUE(ShouldMakeNetworkRequestForURL(GURL("https://foo/bar.html")));
  EXPECT_TRUE(ShouldMakeNetworkRequestForURL(GURL("data://foo")));

  EXPECT_FALSE(ShouldMakeNetworkRequestForURL(GURL("about:blank")));
  EXPECT_FALSE(ShouldMakeNetworkRequestForURL(GURL("about:srcdoc")));
  EXPECT_FALSE(ShouldMakeNetworkRequestForURL(GURL("javascript://foo.js")));
  EXPECT_FALSE(ShouldMakeNetworkRequestForURL(GURL("cid:foo@bar")));
  EXPECT_FALSE(ShouldMakeNetworkRequestForURL(GURL()));
}

}  // namespace content
