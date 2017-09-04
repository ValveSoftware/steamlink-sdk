// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_TEST_UTILS_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_TEST_UTILS_H_

#include <memory>

#include "components/sync/driver/fake_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccountTrackerService;
class FakeSigninManagerBase;
class MockSyncService;
class TestingPrefServiceSimple;
class TestSigninClient;

namespace ntp_snippets {
namespace test {

class FakeSyncService : public syncer::FakeSyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

  bool CanSyncStart() const override;
  bool IsSyncActive() const override;
  bool ConfigurationDone() const override;
  bool IsEncryptEverythingEnabled() const override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;

  bool can_sync_start_;
  bool is_sync_active_;
  bool configuration_done_;
  bool is_encrypt_everything_enabled_;
  syncer::ModelTypeSet active_data_types_;
};

// Common utilities for snippet tests, handles initializing fakes for sync and
// signin.
class NTPSnippetsTestUtils {
 public:
  NTPSnippetsTestUtils();
  ~NTPSnippetsTestUtils();

  void ResetSigninManager();

  FakeSyncService* fake_sync_service() { return fake_sync_service_.get(); }
  FakeSigninManagerBase* fake_signin_manager() {
    return fake_signin_manager_.get();
  }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<FakeSigninManagerBase> fake_signin_manager_;
  std::unique_ptr<FakeSyncService> fake_sync_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
};

}  // namespace test
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_TEST_UTILS_H_
