// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

const char kReloadTestPath[] = "/loader/reload.html";
const char kReloadImagePath[] = "/loader/empty16x16.png";

const char kNoCacheControl[] = "";
const char kMaxAgeCacheControl[] = "max-age=0";
const char kNoCacheCacheControl[] = "no-cache";

struct RequestLog {
  std::string relative_url;
  std::string cache_control;
};

class ReloadCacheControlBrowserTest : public ContentBrowserTest {
 protected:
  ReloadCacheControlBrowserTest() = default;
  ~ReloadCacheControlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // TODO(toyoshim): Tests in this file depend on current reload behavior,
    // and should be modified when we enable the new reload behavior.
    scoped_feature_list_.InitAndDisableFeature(
        features::kNonValidatingReloadOnNormalReload);

    SetUpTestServerOnMainThread();
  }

  void SetUpTestServerOnMainThread() {
    // ContentBrowserTest creates embedded_test_server instance with
    // a registered HandleFileRequest for "content/test/data".
    // Because the handler is registered as the first handler, MonitorHandler
    // is needed to capture all requests.
    embedded_test_server()->RegisterRequestMonitor(base::Bind(
        &ReloadCacheControlBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  std::vector<RequestLog> request_log_;

 private:
  void MonitorRequestHandler(const HttpRequest& request) {
    RequestLog log;
    log.relative_url = request.relative_url;
    auto cache_control = request.headers.find("Cache-Control");
    log.cache_control = cache_control == request.headers.end()
                            ? kNoCacheControl
                            : cache_control->second;
    request_log_.push_back(log);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReloadCacheControlBrowserTest);
};

class ReloadCacheControlWithAnExperimentBrowserTest
    : public ReloadCacheControlBrowserTest {
 protected:
  ReloadCacheControlWithAnExperimentBrowserTest() = default;
  ~ReloadCacheControlWithAnExperimentBrowserTest() override = default;

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kNonValidatingReloadOnNormalReload);

    SetUpTestServerOnMainThread();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReloadCacheControlWithAnExperimentBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, NormalReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  ASSERT_EQ(4UL, request_log_.size());
  EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);

  EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
  EXPECT_EQ(kMaxAgeCacheControl, request_log_[2].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
  EXPECT_EQ(kMaxAgeCacheControl, request_log_[3].cache_control);
}

IN_PROC_BROWSER_TEST_F(ReloadCacheControlBrowserTest, BypassingReload) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ReloadBypassingCacheBlockUntilNavigationsComplete(shell(), 1);

  ASSERT_EQ(4UL, request_log_.size());
  EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);

  EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
  EXPECT_EQ(kNoCacheCacheControl, request_log_[2].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
  EXPECT_EQ(kNoCacheCacheControl, request_log_[3].cache_control);
}

IN_PROC_BROWSER_TEST_F(ReloadCacheControlWithAnExperimentBrowserTest,
                       ReloadMainResource) {
  GURL url(embedded_test_server()->GetURL(kReloadTestPath));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  ASSERT_EQ(4UL, request_log_.size());
  EXPECT_EQ(kReloadTestPath, request_log_[0].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[0].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[1].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[1].cache_control);

  EXPECT_EQ(kReloadTestPath, request_log_[2].relative_url);
  EXPECT_EQ(kMaxAgeCacheControl, request_log_[2].cache_control);
  EXPECT_EQ(kReloadImagePath, request_log_[3].relative_url);
  EXPECT_EQ(kNoCacheControl, request_log_[3].cache_control);
}

// TODO(toyoshim): Add another set of reload tests with DevTools open.

}  // namespace

}  // namespace content
