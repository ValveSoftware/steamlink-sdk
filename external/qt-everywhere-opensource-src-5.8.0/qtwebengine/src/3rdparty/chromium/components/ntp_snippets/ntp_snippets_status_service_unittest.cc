// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_status_service.h"

#include <memory>

#include "components/ntp_snippets/ntp_snippets_test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/fake_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace ntp_snippets {

class NTPSnippetsStatusServiceTest : public test::NTPSnippetsTestBase {
 public:
  void SetUp() override {
    test::NTPSnippetsTestBase::SetUp();

    service_.reset(new NTPSnippetsStatusService(fake_signin_manager(),
                                                mock_sync_service()));
  }

 protected:
  NTPSnippetsStatusService* service() { return service_.get(); }

 private:
  std::unique_ptr<NTPSnippetsStatusService> service_;
};

TEST_F(NTPSnippetsStatusServiceTest, SyncStateCompatibility) {
  // The default test setup is signed out.
  EXPECT_EQ(DisabledReason::SIGNED_OUT, service()->GetDisabledReasonFromDeps());

  // Once signed in, we should be in a compatible sync state.
  fake_signin_manager()->SignIn("foo@bar.com");
  EXPECT_EQ(DisabledReason::NONE, service()->GetDisabledReasonFromDeps());

  // History sync disabled.
  mock_sync_service()->active_data_types_ = syncer::ModelTypeSet();
  EXPECT_EQ(DisabledReason::HISTORY_SYNC_DISABLED,
            service()->GetDisabledReasonFromDeps());

  // Encryption enabled.
  mock_sync_service()->is_encrypt_everything_enabled_ = true;
  EXPECT_EQ(DisabledReason::PASSPHRASE_ENCRYPTION_ENABLED,
            service()->GetDisabledReasonFromDeps());

  // Not done loading.
  mock_sync_service()->configuration_done_ = false;
  mock_sync_service()->active_data_types_ = syncer::ModelTypeSet();
  EXPECT_EQ(DisabledReason::HISTORY_SYNC_STATE_UNKNOWN,
            service()->GetDisabledReasonFromDeps());

  // Sync disabled.
  mock_sync_service()->can_sync_start_ = false;
  EXPECT_EQ(DisabledReason::SYNC_DISABLED,
            service()->GetDisabledReasonFromDeps());
}

}  // namespace ntp_snippets
