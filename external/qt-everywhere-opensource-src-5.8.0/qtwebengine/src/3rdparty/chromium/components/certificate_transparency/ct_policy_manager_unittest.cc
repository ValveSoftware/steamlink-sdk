// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_policy_manager.h"

#include <iterator>

#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_message_loop.h"
#include "base/values.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

template <size_t N>
base::ListValue* ListValueFromStrings(const char* const (&strings)[N]) {
  std::unique_ptr<base::ListValue> result(new base::ListValue);
  for (const auto& str : strings) {
    result->AppendString(str);
  }
  return result.release();
}

class CTPolicyManagerTest : public ::testing::Test {
 public:
  CTPolicyManagerTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

 protected:
  base::TestMessageLoop message_loop_;
  TestingPrefServiceSimple pref_service_;
};

// Treat the preferences as a black box as far as naming, but ensure that
// preferences get registered.
TEST_F(CTPolicyManagerTest, RegistersPrefs) {
  auto registered_prefs = std::distance(pref_service_.registry()->begin(),
                                        pref_service_.registry()->end());
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  auto newly_registered_prefs = std::distance(pref_service_.registry()->begin(),
                                              pref_service_.registry()->end());
  EXPECT_NE(registered_prefs, newly_registered_prefs);
}

TEST_F(CTPolicyManagerTest, DelegateChecksRequired) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  // Register preferences and set up initial state
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  CTPolicyManager manager(&pref_service_, message_loop_.task_runner());
  base::RunLoop().RunUntilIdle();

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No preferences should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com"));

  // Now set a preference, pump the message loop, and ensure things are now
  // reflected.
  pref_service_.SetManagedPref(prefs::kCTRequiredHosts,
                               ListValueFromStrings({"google.com"}));
  base::RunLoop().RunUntilIdle();

  // The new preferences should take effect.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("google.com"));
}

TEST_F(CTPolicyManagerTest, DelegateChecksExcluded) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  // Register preferences and set up initial state
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  CTPolicyManager manager(&pref_service_, message_loop_.task_runner());
  base::RunLoop().RunUntilIdle();

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No preferences should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com"));

  // Now set a preference, pump the message loop, and ensure things are now
  // reflected.
  pref_service_.SetManagedPref(prefs::kCTExcludedHosts,
                               ListValueFromStrings({"google.com"}));
  base::RunLoop().RunUntilIdle();

  // The new preferences should take effect.
  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("google.com"));
}

TEST_F(CTPolicyManagerTest, IgnoresInvalidEntries) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  // Register preferences and set up initial state
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  CTPolicyManager manager(&pref_service_, message_loop_.task_runner());
  base::RunLoop().RunUntilIdle();

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No preferences should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com"));

  // Now setup invalid preferences (that is, that fail to be parsable as
  // URLs).
  pref_service_.SetManagedPref(
      prefs::kCTRequiredHosts,
      ListValueFromStrings({
          "file:///etc/fstab", "file://withahost/etc/fstab",
          "file:///c|/Windows", "*", "https://*", "example.com",
          "https://example.test:invalid_port",
      }));
  base::RunLoop().RunUntilIdle();

  // Wildcards are ignored (both * and https://*).
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com"));
  // File URL hosts are ignored.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("withahost"));

  // While the partially parsed hosts should take effect.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("example.test"));
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("example.com"));
}

// Make sure the various 'undocumented' priorities apply:
//   - non-wildcards beat wildcards
//   - more specific hosts beat less specific hosts
//   - requiring beats excluding
TEST_F(CTPolicyManagerTest, AppliesPriority) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  // Register preferences and set up initial state
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  CTPolicyManager manager(&pref_service_, message_loop_.task_runner());
  base::RunLoop().RunUntilIdle();

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No preferences should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("sub.example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("login.accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("sub.accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("login.sub.accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("test.example.com"));

  // Set up policies that exclude it for a domain and all of its subdomains,
  // but then require it for a specific host.
  pref_service_.SetManagedPref(
      prefs::kCTExcludedHosts,
      ListValueFromStrings({"example.com", ".sub.example.com",
                            ".sub.accounts.example.com", "test.example.com"}));
  pref_service_.SetManagedPref(
      prefs::kCTRequiredHosts,
      ListValueFromStrings(
          {"sub.example.com", "accounts.example.com", "test.example.com"}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("example.com"));
  // Non-wildcarding (.sub.example.com) beats wildcarding (sub.example.com).
  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("sub.example.com"));
  // More specific hosts (accounts.example.com) beat less specific hosts
  // (example.com + wildcard).
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("accounts.example.com"));
  // More specific hosts (accounts.example.com) beat less specific hosts
  // (example.com).
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("login.accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::NOT_REQUIRED,
            delegate->IsCTRequiredForHost("sub.accounts.example.com"));
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("login.sub.accounts.example.com"));
  // Requiring beats excluding.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("test.example.com"));
}

// Ensure that the RequireCTDelegate is still valid and usable after Shutdown
// has been called. Preferences should no longer sync, but the old results
// should still be returned.
TEST_F(CTPolicyManagerTest, UsableAfterShutdown) {
  using CTRequirementLevel =
      net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel;
  // Register preferences and set up initial state
  CTPolicyManager::RegisterPrefs(pref_service_.registry());
  CTPolicyManager manager(&pref_service_, message_loop_.task_runner());
  base::RunLoop().RunUntilIdle();

  net::TransportSecurityState::RequireCTDelegate* delegate =
      manager.GetDelegate();
  ASSERT_TRUE(delegate);

  // No preferences should yield the default results.
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("google.com"));

  // Now set a preference, pump the message loop, and ensure things are now
  // reflected.
  pref_service_.SetManagedPref(prefs::kCTRequiredHosts,
                               ListValueFromStrings({"google.com"}));
  base::RunLoop().RunUntilIdle();

  // The new preferences should take effect.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("google.com"));

  // Shut down the preferences, which should unregister any observers.
  manager.Shutdown();
  base::RunLoop().RunUntilIdle();

  // Update the preferences again, which should do nothing; the
  // RequireCTDelegate should continue to be valid and return the old results.
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("google.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("sub.example.com"));
  pref_service_.SetManagedPref(prefs::kCTRequiredHosts,
                               ListValueFromStrings({"sub.example.com"}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CTRequirementLevel::REQUIRED,
            delegate->IsCTRequiredForHost("google.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("example.com"));
  EXPECT_EQ(CTRequirementLevel::DEFAULT,
            delegate->IsCTRequiredForHost("sub.example.com"));

  // And it should still be possible to get the delegate, even after calling
  // Shutdown().
  EXPECT_TRUE(manager.GetDelegate());
}

}  // namespace

}  // namespace certificate_transparency
