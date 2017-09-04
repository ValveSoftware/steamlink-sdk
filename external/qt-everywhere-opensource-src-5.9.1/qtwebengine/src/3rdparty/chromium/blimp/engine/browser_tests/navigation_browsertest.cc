// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/client/app/session/test_client_session.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/contents/blimp_navigation_controller.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "blimp/client/test/contents/mock_blimp_contents_observer.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "blimp/engine/browser_tests/blimp_browser_test.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;

namespace blimp {
namespace {

const char kPage1Path[] = "/page1.html";
const char kPage2Path[] = "/page2.html";
const char kPage1Title[] = "page1";
const char kPage2Title[] = "page2";

// Tests for Blimp navigation features, with a full engine and a headless
// client. These tests cover Blimp engine interaction with WebContents and
// NavigationController, as well as network interaction between client and
// engine.
class NavigationBrowserTest : public BlimpBrowserTest {
 public:
  NavigationBrowserTest() {}

 protected:
  void SetUpOnMainThread() override {
    BlimpBrowserTest::SetUpOnMainThread();

    client::BlimpClientContext::RegisterPrefs(prefs_.registry());

    context_ = base::WrapUnique<client::BlimpClientContext>(
        client::BlimpClientContext::Create(
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO),
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::FILE),
            base::MakeUnique<client::MockCompositorDependencies>(), &prefs_));

    delegate_ = base::MakeUnique<client::TestBlimpClientContextDelegate>();
    context_->SetDelegate(delegate_.get());
    context_->ConnectWithAssignment(GetAssignment());
    contents_ = context_->CreateBlimpContents(nullptr);

    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    contents_.reset();
    context_.reset();
    delegate_.reset();
    BlimpBrowserTest::TearDownOnMainThread();
  }

  // Given a path on the embedded local server, tell the client to navigate
  // that page by URL.
  //
  // This method does not block; it simply triggers the intended action from the
  // client side. See: RunUntilCompletion, SignalCompletion, and ExpectPageLoad.
  void NavigateToLocalUrl(const std::string& path) {
    GURL url = embedded_test_server()->GetURL(path);
    contents_->GetNavigationController().LoadURL(url);
  }

  // Expect a page load on a mock observer, indicated by the loading state
  // changing to true and then to false.
  //
  // When the page load has completed, the mock will call SignalCompletion,
  // which will cause RunUntilCompletion to return on the test thread.
  void ExpectPageLoad(client::MockBlimpContentsObserver* observer) {
    // There may be redundant events indicating the page load status is true,
    // so allow more than one.
    EXPECT_CALL(*observer, OnLoadingStateChanged(true)).Times(AtLeast(1));
    EXPECT_CALL(*observer, OnLoadingStateChanged(false))
        .WillOnce(
            InvokeWithoutArgs(this, &NavigationBrowserTest::SignalCompletion));
  }

  // Tell the client to navigate to a URL on the local server, and then wait
  // for the page to load.
  void LoadPage(const std::string& path) {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    NavigateToLocalUrl(path);
    RunUntilCompletion();
  }

  // Gets the current page title from the client.
  const std::string& GetTitle() {
    return contents_->GetNavigationController().GetTitle();
  }

  std::unique_ptr<client::BlimpClientContextDelegate> delegate_;
  std::unique_ptr<client::BlimpClientContext> context_;
  std::unique_ptr<client::BlimpContents> contents_;

 private:
  TestingPrefServiceSimple prefs_;
  DISALLOW_COPY_AND_ASSIGN(NavigationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, LoadUrl) {
  LoadPage(kPage1Path);
  EXPECT_EQ(kPage1Title, GetTitle());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, Reload) {
  LoadPage(kPage1Path);
  EXPECT_EQ(kPage1Title, GetTitle());

  {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    contents_->GetNavigationController().Reload();
    RunUntilCompletion();
  }
  EXPECT_EQ(kPage1Title, GetTitle());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, GoBackAndGoForward) {
  LoadPage(kPage1Path);
  EXPECT_EQ(kPage1Title, GetTitle());

  LoadPage(kPage2Path);
  EXPECT_EQ(kPage2Title, GetTitle());

  {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    contents_->GetNavigationController().GoBack();
    RunUntilCompletion();
  }
  EXPECT_EQ(kPage1Title, GetTitle());

  {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    contents_->GetNavigationController().GoForward();
    RunUntilCompletion();
  }
  EXPECT_EQ(kPage2Title, GetTitle());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NavigateAfterInvalidGoBack) {
  // Try an invalid GoBack before loading a page, and assert that the page still
  // loads correctly.
  {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    contents_->GetNavigationController().GoBack();
    NavigateToLocalUrl(kPage1Path);
    RunUntilCompletion();
  }
  EXPECT_EQ(kPage1Title, GetTitle());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NavigateAfterInvalidGoForward) {
  LoadPage(kPage1Path);
  EXPECT_EQ(kPage1Title, GetTitle());

  // Try an invalid GoForward before loading a different page, and
  // assert that the page still loads correctly.
  {
    client::MockBlimpContentsObserver observer(contents_.get());
    ExpectPageLoad(&observer);
    contents_->GetNavigationController().GoForward();
    NavigateToLocalUrl(kPage2Path);
    RunUntilCompletion();
  }
  EXPECT_EQ(kPage2Title, GetTitle());
}

}  // namespace
}  // namespace blimp
