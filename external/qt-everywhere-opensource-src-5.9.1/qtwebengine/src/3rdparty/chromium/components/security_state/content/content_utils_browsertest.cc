// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using content::NavigateToURL;
using security_state::GetVisibleSecurityState;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("components/security_state/content/testdata");

class SecurityStateContentUtilsBrowserTest
    : public content::ContentBrowserTest {
 public:
  SecurityStateContentUtilsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateContentUtilsBrowserTest);
};

// Tests that the NavigationEntry's flags for nonsecure password/credit
// card inputs are reflected in the VisibleSecurityState.
IN_PROC_BROWSER_TEST_F(SecurityStateContentUtilsBrowserTest,
                       VisibleSecurityStateNonsecureFormInputs) {
  ASSERT_TRUE(https_server_.Start());
  EXPECT_TRUE(NavigateToURL(shell(), https_server_.GetURL("/hello.html")));

  content::WebContents* contents = shell()->web_contents();
  ASSERT_TRUE(contents);

  // First, test that if the flags aren't set on the NavigationEntry,
  // then they also aren't set on the VisibleSecurityState.
  content::SSLStatus& ssl_status =
      contents->GetController().GetVisibleEntry()->GetSSL();
  ASSERT_FALSE(ssl_status.content_status &
               content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  ASSERT_FALSE(ssl_status.content_status &
               content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  std::unique_ptr<security_state::VisibleSecurityState>
      visible_security_state_no_sensitive_inputs =
          GetVisibleSecurityState(contents);
  EXPECT_FALSE(visible_security_state_no_sensitive_inputs
                   ->displayed_password_field_on_http);
  EXPECT_FALSE(visible_security_state_no_sensitive_inputs
                   ->displayed_credit_card_field_on_http);

  // Now, set the flags on the NavigationEntry and test that they are
  // reflected in the VisibleSecurityState.
  ssl_status.content_status |=
      content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP;
  ssl_status.content_status |=
      content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP;
  std::unique_ptr<security_state::VisibleSecurityState>
      visible_security_state_sensitive_inputs =
          GetVisibleSecurityState(contents);
  EXPECT_TRUE(visible_security_state_sensitive_inputs
                  ->displayed_password_field_on_http);
  EXPECT_TRUE(visible_security_state_sensitive_inputs
                  ->displayed_credit_card_field_on_http);
}

}  // namespace
