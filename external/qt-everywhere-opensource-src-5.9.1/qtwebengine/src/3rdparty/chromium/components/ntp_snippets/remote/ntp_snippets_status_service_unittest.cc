// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_status_service.h"

#include <memory>

#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/ntp_snippets_test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class NTPSnippetsStatusServiceTest : public ::testing::Test {
 public:
  NTPSnippetsStatusServiceTest() {
    NTPSnippetsStatusService::RegisterProfilePrefs(
        utils_.pref_service()->registry());
  }

  std::unique_ptr<NTPSnippetsStatusService> MakeService() {
    return base::MakeUnique<NTPSnippetsStatusService>(
        utils_.fake_signin_manager(), utils_.pref_service());
  }

 protected:
  test::NTPSnippetsTestUtils utils_;
};

// TODO(jkrcal): Extend the ways to override variation parameters in unit-test
// (bug 645447), and recover the SigninStateCompatibility test that sign-in is
// required when the parameter is overriden.
TEST_F(NTPSnippetsStatusServiceTest, DisabledViaPref) {
  auto service = MakeService();

  // The default test setup is signed out. The service is enabled.
  ASSERT_EQ(SnippetsStatus::ENABLED_AND_SIGNED_OUT,
            service->GetSnippetsStatusFromDeps());

  // Once the enabled pref is set to false, we should be disabled.
  utils_.pref_service()->SetBoolean(prefs::kEnableSnippets, false);
  EXPECT_EQ(SnippetsStatus::EXPLICITLY_DISABLED,
            service->GetSnippetsStatusFromDeps());

  // Signing-in shouldn't matter anymore.
  utils_.fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(SnippetsStatus::EXPLICITLY_DISABLED,
            service->GetSnippetsStatusFromDeps());
}

}  // namespace ntp_snippets
