// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_backoff_manager.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class URLRequestBackoffManagerTest : public testing::Test {
 protected:
  URLRequestBackoffManagerTest() : manager_(new URLRequestBackoffManager) {}
  void RegisterURL(const GURL& url,
                   int backoff_in_sec,
                   const base::Time& request_time) {
    std::string raw_headers = base::StringPrintf(
        "HTTP/1.0 200 OK\n"
        "backoff: %d\n\n",
        backoff_in_sec);
    scoped_refptr<HttpResponseHeaders> headers(new HttpResponseHeaders(
        HttpUtil::AssembleRawHeaders(raw_headers.c_str(), raw_headers.size())));
    manager_->UpdateWithResponse(url, headers.get(), request_time);
  }

  std::unique_ptr<URLRequestBackoffManager> manager_;
};
}  // namespace

TEST_F(URLRequestBackoffManagerTest, ShouldRejectRequest) {
  base::Time request_time = base::Time::Now();
  RegisterURL(GURL("https://example.com"), 3600, request_time);
  ASSERT_EQ(1, manager_->GetNumberOfEntriesForTests());
  ASSERT_TRUE(manager_->ShouldRejectRequest(
      GURL("https://example.com?q=v"),
      request_time + base::TimeDelta::FromSeconds(3500)));

  // Only can try once in the interval of
  // [|request_time| + 3600, |request_time| + 3600 * 1.1).
  ASSERT_FALSE(manager_->ShouldRejectRequest(
      GURL("https://example.com?q=v"),
      request_time + base::TimeDelta::FromSeconds(3700)));
  ASSERT_TRUE(manager_->ShouldRejectRequest(
      GURL("https://example.com?q=v"),
      request_time + base::TimeDelta::FromSeconds(3700)));
  ASSERT_EQ(1, manager_->GetNumberOfEntriesForTests());

  // After release time, throttling should not be applied.
  ASSERT_FALSE(manager_->ShouldRejectRequest(
      GURL("https://example.com?q=v"),
      request_time + base::TimeDelta::FromSeconds(3960)));
}

TEST_F(URLRequestBackoffManagerTest, MisconfiguredHeaders) {
  // Backoff time is smaller than the minimum allowed.
  RegisterURL(GURL("https://example.com"),
              URLRequestBackoffManager::kMinimumBackoffInSeconds - 1,
              base::Time::Now());
  ASSERT_EQ(0, manager_->GetNumberOfEntriesForTests());

  // Backoff time is bigger than the maximum allowed.
  RegisterURL(GURL("https://example.com"),
              URLRequestBackoffManager::kMaximumBackoffInSeconds + 1,
              base::Time::Now());
  ASSERT_EQ(0, manager_->GetNumberOfEntriesForTests());
}

TEST_F(URLRequestBackoffManagerTest, ShouldGarbageCollect) {
  base::Time request_time =
      base::Time::Now() - base::TimeDelta::FromSeconds(60);
  for (int i = 0;
       i < URLRequestBackoffManager::kNewEntriesBetweenCollecting - 1; i++) {
    RegisterURL(GURL(base::StringPrintf("http://example%d.com", i)), 10,
                request_time);
    ASSERT_EQ(i + 1, manager_->GetNumberOfEntriesForTests());
  }
  // Should clear all previous outdated entries.
  RegisterURL(GURL("http://example.com"), 10, base::Time::Now());
  ASSERT_EQ(1, manager_->GetNumberOfEntriesForTests());
}

TEST_F(URLRequestBackoffManagerTest, ClearOnNetworkChange) {
  for (int i = 0; i < 3; ++i) {
    RegisterURL(GURL("http://www.example.com/"), 60, base::Time::Now());
    ASSERT_EQ(1, manager_->GetNumberOfEntriesForTests());
    EXPECT_TRUE(manager_->ShouldRejectRequest(GURL("http://www.example.com/"),
                                              base::Time::Now()));
    switch (i) {
      case 0:
        manager_->OnIPAddressChanged();
        break;
      case 1:
        manager_->OnConnectionTypeChanged(
            NetworkChangeNotifier::CONNECTION_UNKNOWN);
        break;
      case 2:
        manager_->OnConnectionTypeChanged(
            NetworkChangeNotifier::CONNECTION_NONE);
        break;
      default:
        FAIL();
    }

    EXPECT_FALSE(manager_->ShouldRejectRequest(GURL("http://www.example.com/"),
                                               base::Time::Now()));
    ASSERT_EQ(0, manager_->GetNumberOfEntriesForTests());
  }
}

}  // namespace net
