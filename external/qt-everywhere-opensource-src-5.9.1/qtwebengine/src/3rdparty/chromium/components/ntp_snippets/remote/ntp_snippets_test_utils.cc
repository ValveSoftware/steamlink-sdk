// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippets_test_utils.h"

#include <memory>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync/driver/fake_sync_service.h"

namespace ntp_snippets {
namespace test {

FakeSyncService::FakeSyncService()
    : can_sync_start_(true),
      is_sync_active_(true),
      configuration_done_(true),
      is_encrypt_everything_enabled_(false),
      active_data_types_(syncer::HISTORY_DELETE_DIRECTIVES) {}

FakeSyncService::~FakeSyncService() = default;

bool FakeSyncService::CanSyncStart() const {
  return can_sync_start_;
}

bool FakeSyncService::IsSyncActive() const {
  return is_sync_active_;
}

bool FakeSyncService::ConfigurationDone() const {
  return configuration_done_;
}

bool FakeSyncService::IsEncryptEverythingEnabled() const {
  return is_encrypt_everything_enabled_;
}

syncer::ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return active_data_types_;
}

NTPSnippetsTestUtils::NTPSnippetsTestUtils()
    : pref_service_(new TestingPrefServiceSimple()) {
  pref_service_->registry()->RegisterStringPref(prefs::kGoogleServicesAccountId,
                                                std::string());
  pref_service_->registry()->RegisterStringPref(
      prefs::kGoogleServicesLastAccountId, std::string());
  pref_service_->registry()->RegisterStringPref(
      prefs::kGoogleServicesLastUsername, std::string());
  signin_client_.reset(new TestSigninClient(pref_service_.get()));
  account_tracker_.reset(new AccountTrackerService());
  fake_sync_service_.reset(new FakeSyncService());
  ResetSigninManager();
}

NTPSnippetsTestUtils::~NTPSnippetsTestUtils() = default;

void NTPSnippetsTestUtils::ResetSigninManager() {
  fake_signin_manager_.reset(
      new FakeSigninManagerBase(signin_client_.get(), account_tracker_.get()));
}

}  // namespace test
}  // namespace ntp_snippets
