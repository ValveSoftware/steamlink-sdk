// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/client/feature/ime_feature.h"
#include "blimp/client/feature/mock_ime_feature_delegate.h"
#include "blimp/client/feature/mock_navigation_feature_delegate.h"
#include "blimp/client/feature/mock_render_widget_feature_delegate.h"
#include "blimp/client/feature/navigation_feature.h"
#include "blimp/client/feature/render_widget_feature.h"
#include "blimp/client/feature/tab_control_feature.h"
#include "blimp/client/session/assignment_source.h"
#include "blimp/client/session/test_client_session.h"
#include "blimp/engine/browser_tests/blimp_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using ::testing::InvokeWithoutArgs;

namespace blimp {
namespace {

const int kDummyTabId = 0;

// Uses a headless client session to test a full engine.
class EngineBrowserTest : public BlimpBrowserTest {
 public:
  EngineBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    BlimpBrowserTest::SetUpOnMainThread();

    // Create a headless client on UI thread.
    client_session_.reset(new client::TestClientSession);

    // Set feature delegates.
    client_session_->GetNavigationFeature()->SetDelegate(
        kDummyTabId, &client_nav_feature_delegate_);
    client_session_->GetRenderWidgetFeature()->SetDelegate(
        kDummyTabId, &client_rw_feature_delegate_);
    client_session_->GetImeFeature()->set_delegate(
        &client_ime_feature_delegate_);
  }

  client::MockNavigationFeatureDelegate client_nav_feature_delegate_;
  client::MockRenderWidgetFeatureDelegate client_rw_feature_delegate_;
  client::MockImeFeatureDelegate client_ime_feature_delegate_;
  std::unique_ptr<client::TestClientSession> client_session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EngineBrowserTest);
};

IN_PROC_BROWSER_TEST_F(EngineBrowserTest, LoadUrl) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/hello.html");

  EXPECT_CALL(client_rw_feature_delegate_, OnRenderWidgetCreated(1));
  EXPECT_CALL(client_nav_feature_delegate_, OnUrlChanged(kDummyTabId, url))
      .WillOnce(InvokeWithoutArgs(this, &EngineBrowserTest::QuitRunLoop));

  // Skip assigner. Engine info is already available.
  client_session_->ConnectWithAssignment(
      client::AssignmentSource::Result::RESULT_OK, GetAssignment());
  client_session_->GetTabControlFeature()->SetSizeAndScale(gfx::Size(100, 100),
                                                           1);
  client_session_->GetTabControlFeature()->CreateTab(kDummyTabId);
  client_session_->GetNavigationFeature()->NavigateToUrlText(kDummyTabId,
                                                             url.spec());

  RunUntilQuit();
}

}  // namespace
}  // namespace blimp
