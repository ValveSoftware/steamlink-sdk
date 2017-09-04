// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include "base/macros.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A WebContentsDelegate that exposes the visible SSLStatus at the time
// of the last VisibleSecurityStateChanged() call.
class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : WebContentsDelegate() {}
  ~TestWebContentsDelegate() override {}

  const SSLStatus& last_ssl_state() { return last_ssl_state_; }

  // WebContentsDelegate:
  void VisibleSecurityStateChanged(WebContents* source) override {
    NavigationEntry* entry = source->GetController().GetVisibleEntry();
    EXPECT_TRUE(entry);
    last_ssl_state_ = entry->GetSSL();
  }

 private:
  SSLStatus last_ssl_state_;
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsDelegate);
};

class SSLManagerTest : public RenderViewHostTestHarness {
 public:
  SSLManagerTest() : RenderViewHostTestHarness() {}
  ~SSLManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLManagerTest);
};

// Tests that VisibleSecurityStateChanged() is called when a password input
// is shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnHttpPassword) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  web_contents()->OnPasswordInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that VisibleSecurityStateChanged() is called when a credit card input
// is shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnHttpCreditCard) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  web_contents()->OnCreditCardInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
}

// Tests that VisibleSecurityStateChanged() is called when password and
// credit card inputs are shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnPasswordAndHttpCreditCard) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  web_contents()->OnPasswordInputShownOnHttp();
  web_contents()->OnCreditCardInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
}

}  // namespace

}  // namespace content
