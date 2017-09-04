// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/core/browser/stub_log_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace password_manager {

namespace {

class MockLogManager : public StubLogManager {
 public:
  MOCK_CONST_METHOD0(IsLoggingActive, bool(void));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetLogManager, const LogManager*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  FakePasswordAutofillAgent()
      : called_set_logging_state_(false),
        logging_state_active_(false),
        binding_(this) {}

  ~FakePasswordAutofillAgent() override {}

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(mojo::MakeRequest<autofill::mojom::PasswordAutofillAgent>(
        std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

 private:
  // autofill::mojom::PasswordAutofillAgent:
  void FillPasswordForm(
      int key,
      const autofill::PasswordFormFillData& form_data) override {}

  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  void AutofillUsernameAndPasswordDataReceived(
      const autofill::FormsPredictionsMap& predictions) override {}

  void FindFocusedPasswordForm(
      const FindFocusedPasswordFormCallback& callback) override {}

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_;

  mojo::Binding<autofill::mojom::PasswordAutofillAgent> binding_;
};

}  // namespace

class ContentPasswordManagerDriverTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(password_manager_client_, GetLogManager())
        .WillByDefault(Return(&log_manager_));

    service_manager::InterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteInterfaces();
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(
        autofill::mojom::PasswordAutofillAgent::Name_,
        base::Bind(&FakePasswordAutofillAgent::BindRequest,
                   base::Unretained(&fake_agent_)));
  }

  bool WasLoggingActivationMessageSent(bool* activation_flag) {
    base::RunLoop().RunUntilIdle();
    if (!fake_agent_.called_set_logging_state())
      return false;

    if (activation_flag)
      *activation_flag = fake_agent_.logging_state_active();
    fake_agent_.reset_data();
    return true;
  }

 protected:
  MockLogManager log_manager_;
  MockPasswordManagerClient password_manager_client_;
  autofill::TestAutofillClient autofill_client_;

  FakePasswordAutofillAgent fake_agent_;
};

TEST_P(ContentPasswordManagerDriverTest,
       AnswerToNotificationsAboutLoggingState) {
  const bool should_allow_logging = GetParam();
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  driver->SendLoggingAvailability();
  if (should_allow_logging) {
    bool logging_activated = false;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_TRUE(logging_activated);
  } else {
    bool logging_activated = true;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_FALSE(logging_activated);
  }
}

TEST_P(ContentPasswordManagerDriverTest, AnswerToIPCPingsAboutLoggingState) {
  const bool should_allow_logging = GetParam();
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  driver->SendLoggingAvailability();
  WasLoggingActivationMessageSent(nullptr);

  // Ping the driver for logging activity update.
  driver->PasswordAutofillAgentConstructed();

  bool logging_activated = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
  EXPECT_EQ(should_allow_logging, logging_activated);
}

// Tests that password visibility notifications are forwarded to the
// WebContents.
TEST_P(ContentPasswordManagerDriverTest, PasswordVisibility) {
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  // Do a mock navigation so that there is a navigation entry on which
  // password visibility gets recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  driver->PasswordFieldVisibleInInsecureContext();

  // Check that the password visibility notification was passed on to
  // the WebContents (and from there to the SSLStatus).
  entry = web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  // If the password field becomes hidden, then the flag should be unset
  // on the SSLStatus.
  driver->AllPasswordFieldsInInsecureContextInvisible();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
}

// Tests that password visibility notifications from subframes are
// recorded correctly.
TEST_P(ContentPasswordManagerDriverTest, PasswordVisibilityWithSubframe) {
  // Do a mock navigation so that there is a navigation entry on which
  // password visibility gets recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);

  // Create a subframe with a password field and check that
  // notifications for it are handled properly.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto subframe_driver = base::MakeUnique<ContentPasswordManagerDriver>(
      subframe, &password_manager_client_, &autofill_client_);
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL("http://example2.test"));
  subframe_tester->SimulateNavigationCommit(GURL("http://example2.test"));
  subframe_driver->PasswordFieldVisibleInInsecureContext();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  subframe_driver->AllPasswordFieldsInInsecureContextInvisible();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
}

// Tests that password visibility notifications are recorded correctly
// when there is a password field in both the main frame and a subframe.
TEST_P(ContentPasswordManagerDriverTest,
       PasswordVisibilityWithMainFrameAndSubframe) {
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  // Do a mock navigation so that there is a navigation entry on which
  // password visibility gets recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);

  driver->PasswordFieldVisibleInInsecureContext();
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  // Create a subframe with a password field and check that
  // notifications for it are handled properly.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto subframe_driver = base::MakeUnique<ContentPasswordManagerDriver>(
      subframe, &password_manager_client_, &autofill_client_);
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL("http://example2.test"));
  subframe_tester->SimulateNavigationCommit(GURL("http://example2.test"));
  subframe_driver->PasswordFieldVisibleInInsecureContext();

  entry = web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  subframe_driver->AllPasswordFieldsInInsecureContextInvisible();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  driver->AllPasswordFieldsInInsecureContextInvisible();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
}

// Tests that when a frame is deleted, its password visibility flag gets
// unset.
TEST_P(ContentPasswordManagerDriverTest,
       PasswordVisibilityWithSubframeDeleted) {
  // Do a mock navigation so that there is a navigation entry on which
  // password visibility gets recorded.
  GURL url("http://example.test");
  NavigateAndCommit(url);

  // Create a subframe with a password field.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto subframe_driver = base::MakeUnique<ContentPasswordManagerDriver>(
      subframe, &password_manager_client_, &autofill_client_);
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL("http://example2.test"));
  subframe_tester->SimulateNavigationCommit(GURL("http://example2.test"));
  subframe_driver->PasswordFieldVisibleInInsecureContext();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  subframe_tester->Detach();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  // Check that the subframe's flag isn't hanging around preventing the
  // warning from being removed.
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  driver->PasswordFieldVisibleInInsecureContext();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
  driver->AllPasswordFieldsInInsecureContextInvisible();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
}

// Tests that a cross-process navigation does not remove the password
// field flag when the RenderFrameHost for the original process goes
// away. Regression test for https://crbug.com/664674
TEST_F(ContentPasswordManagerDriverTest,
       RenderFrameHostDeletionOnCrossProcessNavigation) {
  NavigateAndCommit(GURL("http://example.test"));

  content::RenderFrameHostTester* old_rfh_tester =
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame());
  content::WebContentsTester* tester =
      content::WebContentsTester::For(web_contents());

  controller().LoadURL(GURL("http://example2.test"), content::Referrer(),
                       ui::PAGE_TRANSITION_TYPED, std::string());
  content::RenderFrameHost* pending_rfh = tester->GetPendingMainFrame();
  ASSERT_TRUE(pending_rfh);
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  tester->TestDidNavigate(pending_rfh, entry_id, true,
                          GURL("http://example2.test"),
                          ui::PAGE_TRANSITION_TYPED);

  auto driver = base::MakeUnique<ContentPasswordManagerDriver>(
      main_rfh(), &password_manager_client_, &autofill_client_);
  driver->PasswordFieldVisibleInInsecureContext();
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));

  // After the old RenderFrameHost is deleted, the password flag should still be
  // set.
  old_rfh_tester->SimulateSwapOutACK();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP));
}

INSTANTIATE_TEST_CASE_P(,
                        ContentPasswordManagerDriverTest,
                        testing::Values(true, false));

}  // namespace password_manager
