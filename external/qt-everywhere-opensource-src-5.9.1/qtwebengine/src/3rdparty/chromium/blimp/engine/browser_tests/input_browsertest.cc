// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/client/app/session/test_client_session.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/mock_ime_feature_delegate.h"
#include "blimp/client/core/contents/mock_navigation_feature_delegate.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/public/session/assignment.h"
#include "blimp/engine/browser_tests/blimp_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace blimp {
namespace {

const int kDummyTabId = 0;
const char kInputPagePath[] = "/input.html";

// TODO(bgoldman): Convert to v0.6 as in the rendering and navigation tests.
class InputBrowserTest : public BlimpBrowserTest {
 public:
  InputBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    BlimpBrowserTest::SetUpOnMainThread();

    // Create a headless client on UI thread.
    client_session_.reset(new client::TestClientSession);

    // Set feature delegates.
    client_session_->GetNavigationFeature()->SetDelegate(
        kDummyTabId, &client_nav_feature_delegate_);
    client_session_->GetImeFeature()->set_delegate(
        &client_ime_feature_delegate_);

    // Skip assigner. Engine info is already available.
    client_session_->ConnectWithAssignment(client::ASSIGNMENT_REQUEST_RESULT_OK,
                                           GetAssignment());
    client_session_->GetTabControlFeature()->SetSizeAndScale(
        gfx::Size(100, 100), 1);
    client_session_->GetTabControlFeature()->CreateTab(kDummyTabId);

    // Record the last page title known to the client. Page loads will sometimes
    // involve multiple title changes in transition, including the requested
    // URL. When a page is done loading, the last title should be the one from
    // the <title> tag.
    ON_CALL(client_nav_feature_delegate_, OnTitleChanged(kDummyTabId, _))
        .WillByDefault(SaveArg<1>(&last_page_title_));

    EXPECT_TRUE(embedded_test_server()->Start());
  }

  // Given a path on the embedded local server, tell the client to navigate to
  // that page by URL.
  void NavigateToLocalUrl(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);
    client_session_->GetNavigationFeature()->NavigateToUrlText(kDummyTabId,
                                                               url.spec());
  }

  // Set mock expectations that a page will load. A page has loaded when the
  // page load status changes to true, and then to false.
  void ExpectPageLoad() {
    testing::InSequence s;

    // There may be redundant events indicating the page load status is true,
    // so allow more than one.
    EXPECT_CALL(client_nav_feature_delegate_,
                OnLoadingChanged(kDummyTabId, true))
        .Times(AtLeast(1));
    EXPECT_CALL(client_nav_feature_delegate_,
                OnLoadingChanged(kDummyTabId, false))
        .WillOnce(InvokeWithoutArgs(this, &InputBrowserTest::SignalCompletion));
  }

  void RunAndVerify() {
    RunUntilCompletion();
    testing::Mock::VerifyAndClearExpectations(&client_nav_feature_delegate_);
    testing::Mock::VerifyAndClearExpectations(&client_ime_feature_delegate_);
  }

  // Tell the client to navigate to a URL on the local server, and then wait
  // for the page to load.
  void LoadPage(const std::string& path) {
    ExpectPageLoad();
    NavigateToLocalUrl(path);
    RunAndVerify();
  }

  client::MockNavigationFeatureDelegate client_nav_feature_delegate_;
  client::MockImeFeatureDelegate client_ime_feature_delegate_;
  std::unique_ptr<client::TestClientSession> client_session_;
  std::string last_page_title_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputBrowserTest);
};

IN_PROC_BROWSER_TEST_F(InputBrowserTest, InputText) {
  LoadPage(kInputPagePath);

  blink::WebGestureEvent event;
  event.type = blink::WebInputEvent::Type::GestureTap;
  client::ImeFeature::WebInputRequest request;

  // Send a tap event from the client and expect the IME dialog to show.
  EXPECT_CALL(client_ime_feature_delegate_, OnShowImeRequested(_))
      .Times(AtLeast(1))
      .WillOnce(
          DoAll(InvokeWithoutArgs(this, &InputBrowserTest::SignalCompletion),
                SaveArg<0>(&request)));
  client_session_->GetRenderWidgetFeature()->SendWebGestureEvent(kDummyTabId, 1,
                                                                 event);
  RunAndVerify();

  // Enter text from the client and expect the input.html JavaScript to update
  // the page title.
  EXPECT_CALL(client_nav_feature_delegate_, OnTitleChanged(kDummyTabId, "test"))
      .WillOnce(InvokeWithoutArgs(this, &InputBrowserTest::SignalCompletion));

  client::ImeFeature::WebInputResponse response;
  response.text = "test";
  response.submit = false;
  request.show_ime_callback.Run(response);
  RunAndVerify();
}

IN_PROC_BROWSER_TEST_F(InputBrowserTest, InputTextAndSubmit) {
  LoadPage(kInputPagePath);

  blink::WebGestureEvent event;
  event.type = blink::WebInputEvent::Type::GestureTap;
  client::ImeFeature::WebInputRequest request;

  // Send a tap event from the client and expect the IME dialog to show.
  EXPECT_CALL(client_ime_feature_delegate_, OnShowImeRequested(_))
      .Times(AtLeast(1))
      .WillOnce(
          DoAll(InvokeWithoutArgs(this, &InputBrowserTest::SignalCompletion),
                SaveArg<0>(&request)));
  client_session_->GetRenderWidgetFeature()->SendWebGestureEvent(kDummyTabId, 1,
                                                                 event);
  RunAndVerify();

  // Enter text from client and submit the form.
  client::ImeFeature::WebInputResponse response;
  response.text = "test";
  response.submit = true;
  request.show_ime_callback.Run(response);

  content::DOMMessageQueue queue;
  std::string status;
  EXPECT_TRUE(queue.WaitForMessage(&status));
  EXPECT_EQ(status, "\"Submitted\"");
}

}  // namespace
}  // namespace blimp
