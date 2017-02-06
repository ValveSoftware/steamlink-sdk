// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/web_restrictions/browser/mock_web_restrictions_client.h"
#include "components/web_restrictions/browser/web_restrictions_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using web_restrictions::WebRestrictionsClient;
using web_restrictions::MockWebRestrictionsClient;

namespace {

bool g_returned_result;

void ResultCallback(const base::Closure& quit_closure, bool result) {
  g_returned_result = result;
  quit_closure.Run();
}

}  // namespace

class WebRestrictionsClientTest : public testing::Test {
 protected:
  // Mock the Java WebRestrictionsClient. The real version
  // would need a content provider to do anything.
  MockWebRestrictionsClient mock_;
  content::TestBrowserThreadBundle thread_bundle_;
  WebRestrictionsClient client_;
};

TEST_F(WebRestrictionsClientTest, ShouldProceed) {
  client_.SetAuthority("Good");
  // First call should go to Web Restrictions Content Provider, and return a
  // delayed result.
  {
    g_returned_result = false;
    base::RunLoop run_loop;
    EXPECT_EQ(web_restrictions::PENDING,
              client_.ShouldProceed(
                  true, GURL("http://example.com"),
                  base::Bind(&ResultCallback, run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(g_returned_result);
    EXPECT_EQ(1, client_.GetResultColumnCount(GURL("http://example.com")));
  }
  // A repeated call should go to the cache and return a result immediately.
  {
    base::RunLoop run_loop;
    EXPECT_EQ(web_restrictions::ALLOW,
              client_.ShouldProceed(
                  true, GURL("http://example.com"),
                  base::Bind(&ResultCallback, run_loop.QuitClosure())));
  }
  // However a different url should miss the cache
  {
    g_returned_result = false;
    base::RunLoop run_loop;
    EXPECT_EQ(web_restrictions::PENDING,
              client_.ShouldProceed(
                  true, GURL("http://example.com/2"),
                  base::Bind(&ResultCallback, run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(g_returned_result);
  }
  // Switching the authority should clear the cache.
  {
    client_.SetAuthority("Good2");
    g_returned_result = false;
    base::RunLoop run_loop;
    EXPECT_EQ(web_restrictions::PENDING,
              client_.ShouldProceed(
                  true, GURL("http://example.com/2"),
                  base::Bind(&ResultCallback, run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(g_returned_result);
  }
  // Try getting a bad result
  {
    client_.SetAuthority("Bad");
    g_returned_result = true;
    base::RunLoop run_loop;
    EXPECT_EQ(web_restrictions::PENDING,
              client_.ShouldProceed(
                  true, GURL("http://example.com/2"),
                  base::Bind(&ResultCallback, run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_FALSE(g_returned_result);
    EXPECT_EQ(3, client_.GetResultColumnCount(GURL("http://example.com/2")));
    EXPECT_EQ(42, client_.GetResultIntValue(GURL("http://example.com/2"), 1));
    EXPECT_EQ("Error string",
              client_.GetResultColumnName(GURL("http://example.com/2"), 2));
    EXPECT_EQ("http://example.com/2",
              client_.GetResultStringValue(GURL("http://example.com/2"), 2));
  }
}

TEST_F(WebRestrictionsClientTest, RequestPermission) {
  client_.SetAuthority("Good");
  {
    base::RunLoop run_loop;
    g_returned_result = false;
    client_.RequestPermission(
        GURL("http://example.com"),
        base::Bind(&ResultCallback, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(g_returned_result);
    client_.SetAuthority("Bad");
  }
  {
    base::RunLoop run_loop;
    g_returned_result = true;
    client_.RequestPermission(
        GURL("http://example.com"),
        base::Bind(&ResultCallback, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_FALSE(g_returned_result);
  }
}
