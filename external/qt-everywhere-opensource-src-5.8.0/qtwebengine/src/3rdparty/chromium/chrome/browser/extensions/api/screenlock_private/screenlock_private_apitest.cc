// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

const char kTestGaiaId[] = "gaia-id-testuser@gmail.com";
const char kAttemptClickAuthMessage[] = "attemptClickAuth";
const char kTestExtensionId[] = "lkegkdgachcnekllcdfkijonogckdnjo";
const char kTestUser[] = "testuser@gmail.com";

}  // namespace

class ScreenlockPrivateApiTest : public ExtensionApiTest,
                                 public content::NotificationObserver {
 public:
  ScreenlockPrivateApiTest() {}

  ~ScreenlockPrivateApiTest() override {}

  // ExtensionApiTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestExtensionId);

#if !defined(OS_CHROMEOS)
    // New profile management needs to be on for non-ChromeOS lock.
    ::switches::EnableNewProfileManagementForTesting(command_line);
#endif
  }

  void SetUpOnMainThread() override {
    SigninManagerFactory::GetForProfile(profile())
        ->SetAuthenticatedAccountInfo(kTestGaiaId, kTestUser);
    ProfileAttributesEntry* entry;
    ASSERT_TRUE(g_browser_process->profile_manager()->
        GetProfileAttributesStorage().
        GetProfileAttributesWithPath(profile()->GetPath(), &entry));
    entry->SetAuthInfo(
        kTestGaiaId, base::UTF8ToUTF16(test_account_id_.GetUserEmail()));
    ExtensionApiTest::SetUpOnMainThread();
  }

 protected:
  // ExtensionApiTest override:
  void RunTestOnMainThreadLoop() override {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
    ExtensionApiTest::RunTestOnMainThreadLoop();
    registrar_.RemoveAll();
  }

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    const std::string& message =
        content::Details<std::pair<std::string, bool*>>(details).ptr()->first;
    if (message == kAttemptClickAuthMessage) {
      proximity_auth::ScreenlockBridge::Get()->lock_handler()->SetAuthType(
          test_account_id_,
          proximity_auth::ScreenlockBridge::LockHandler::USER_CLICK,
          base::string16());
      EasyUnlockService::Get(profile())->AttemptAuth(test_account_id_);
    }
  }

  // Loads |extension_name| and waits for a pass / fail notification.
  void RunTest(const std::string& extension_name) {
    ASSERT_TRUE(RunComponentExtensionTest(extension_name)) << message_;
  }

 private:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUser, kTestGaiaId);
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateApiTest);
};

// Locking is currently implemented only on ChromeOS.
#if defined(OS_CHROMEOS)

// Flaky under MSan. http://crbug.com/478091
#if defined(MEMORY_SANITIZER)
#define MAYBE_LockUnlock DISABLED_LockUnlock
#else
#define MAYBE_LockUnlock LockUnlock
#endif

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, MAYBE_LockUnlock) {
  RunTest("screenlock_private/lock_unlock");
}

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, AuthType) {
  RunTest("screenlock_private/auth_type");
}

#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
