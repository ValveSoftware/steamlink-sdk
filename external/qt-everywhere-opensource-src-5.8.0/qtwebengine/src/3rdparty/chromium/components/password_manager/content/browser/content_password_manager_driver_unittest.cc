// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <stdint.h>

#include <tuple>

#include "base/macros.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/core/browser/stub_log_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
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

}  // namespace

class ContentPasswordManagerDriverTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(password_manager_client_, GetLogManager())
        .WillByDefault(Return(&log_manager_));
  }

  bool WasLoggingActivationMessageSent(bool* activation_flag) {
    const uint32_t kMsgID = AutofillMsg_SetLoggingState::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    std::tuple<bool> param;
    AutofillMsg_SetLoggingState::Read(message, &param);
    *activation_flag = std::get<0>(param);
    process()->sink().ClearMessages();
    return true;
  }

 protected:
  MockLogManager log_manager_;
  MockPasswordManagerClient password_manager_client_;
  autofill::TestAutofillClient autofill_client_;
};

TEST_P(ContentPasswordManagerDriverTest,
       AnswerToNotificationsAboutLoggingState) {
  const bool should_allow_logging = GetParam();
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  process()->sink().ClearMessages();

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
  process()->sink().ClearMessages();

  // Ping the driver for logging activity update.
  AutofillHostMsg_PasswordAutofillAgentConstructed msg(0);
  driver->HandleMessage(msg);

  bool logging_activated = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
  EXPECT_EQ(should_allow_logging, logging_activated);
}

INSTANTIATE_TEST_CASE_P(,
                        ContentPasswordManagerDriverTest,
                        testing::Values(true, false));

}  // namespace password_manager
