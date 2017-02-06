// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <sstream>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

static const size_t kNumMocks = 3;
static const int kNumCharactersInPassword = 10;
static const char kPlaintextPassword[] = "plaintext";

api::passwords_private::PasswordUiEntry CreateEntry(size_t num) {
  api::passwords_private::PasswordUiEntry entry;
  std::stringstream ss;
  ss << "http://test" << num << ".com";
  entry.login_pair.origin_url = ss.str();
  ss << "/login";
  entry.link_url = ss.str();
  ss.clear();
  ss << "testName" << num;
  entry.login_pair.username = ss.str();
  entry.num_characters_in_password = kNumCharactersInPassword;
  return entry;
}

api::passwords_private::ExceptionPair CreateException(size_t num) {
  api::passwords_private::ExceptionPair exception;
  std::stringstream ss;
  ss << "http://exception" << num << ".com";
  exception.exception_url = ss.str();
  ss << "/login";
  exception.link_url = ss.str();
  return exception;
}

// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveSavedPassword()
// or RemovePasswordException() is called.
class TestDelegate : public PasswordsPrivateDelegate {
 public:
  TestDelegate() : profile_(nullptr) {
    // Create mock data.
    for (size_t i = 0; i < kNumMocks; i++) {
      current_entries_.push_back(CreateEntry(i));
      current_exceptions_.push_back(CreateException(i));
    }
  }
  ~TestDelegate() override {}

  void SendSavedPasswordsList() override {
    PasswordsPrivateEventRouter* router =
        PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
    if (router)
      router->OnSavedPasswordsListChanged(current_entries_);
  }

  const std::vector<api::passwords_private::PasswordUiEntry>*
  GetSavedPasswordsList() const override {
    return &current_entries_;
  }

  void SendPasswordExceptionsList() override {
    PasswordsPrivateEventRouter* router =
        PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
    if (router)
      router->OnPasswordExceptionsListChanged(current_exceptions_);
  }

  const std::vector<api::passwords_private::ExceptionPair>*
  GetPasswordExceptionsList() const override {
    return &current_exceptions_;
  }

  void RemoveSavedPassword(
      const std::string& origin_url, const std::string& username) override {
    if (current_entries_.empty())
      return;

    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    current_entries_.erase(current_entries_.begin());
    SendSavedPasswordsList();
  }

  void RemovePasswordException(const std::string& exception_url) override {
    if (current_exceptions_.empty())
      return;

    // Since this is just mock data, remove the first entry regardless of
    // the data contained.
    current_exceptions_.erase(current_exceptions_.begin());
    SendPasswordExceptionsList();
  }

  void RequestShowPassword(const std::string& origin_url,
                           const std::string& username,
                           content::WebContents* web_contents) override {
    // Return a mocked password value.
    std::string plaintext_password(kPlaintextPassword);
    PasswordsPrivateEventRouter* router =
        PasswordsPrivateEventRouterFactory::GetForProfile(profile_);
    if (router) {
      router->OnPlaintextPasswordFetched(origin_url, username,
                                         plaintext_password);
    }
  }

  void SetProfile(Profile* profile) { profile_ = profile; }

 private:
  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<api::passwords_private::PasswordUiEntry> current_entries_;
  std::vector<api::passwords_private::ExceptionPair> current_exceptions_;
  Profile* profile_;
};

class PasswordsPrivateApiTest : public ExtensionApiTest {
 public:
  PasswordsPrivateApiTest() {
    if (!s_test_delegate_) {
      s_test_delegate_ = new TestDelegate();
    }
  }
  ~PasswordsPrivateApiTest() override {}

  static std::unique_ptr<KeyedService> GetPasswordsPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return base::WrapUnique(s_test_delegate_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUp() override {
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    PasswordsPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), &PasswordsPrivateApiTest::GetPasswordsPrivateDelegate);
    s_test_delegate_->SetProfile(profile());
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunPasswordsSubtest(const std::string& subtest) {
    return RunExtensionSubtest("passwords_private",
                               "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

 private:
  static TestDelegate* s_test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateApiTest);
};

// static
TestDelegate* PasswordsPrivateApiTest::s_test_delegate_ = nullptr;

}  // namespace

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RemoveSavedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("removeSavedPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RemovePasswordException) {
  EXPECT_TRUE(RunPasswordsSubtest("removePasswordException")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetSavedPasswordList) {
  EXPECT_TRUE(RunPasswordsSubtest("getSavedPasswordList")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetPasswordExceptionList) {
  EXPECT_TRUE(RunPasswordsSubtest("getPasswordExceptionList")) << message_;
}

}  // namespace extensions
