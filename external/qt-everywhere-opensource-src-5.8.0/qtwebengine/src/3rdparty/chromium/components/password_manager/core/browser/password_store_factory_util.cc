// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_factory_util.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

namespace {

bool ShouldAffiliationBasedMatchingBeActive(
    sync_driver::SyncService* sync_service) {
  return base::FeatureList::IsEnabled(features::kAffiliationBasedMatching) &&
         sync_service && sync_service->CanSyncStart() &&
         sync_service->IsSyncActive() &&
         sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS) &&
         !sync_service->IsUsingSecondaryPassphrase();
}

void ActivateAffiliationBasedMatching(
    PasswordStore* password_store,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& db_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  // The PasswordStore is so far the only consumer of the AffiliationService,
  // therefore the service is owned by the AffiliatedMatchHelper, which in
  // turn is owned by the PasswordStore.
  std::unique_ptr<AffiliationService> affiliation_service(
      new AffiliationService(db_thread_runner));
  affiliation_service->Initialize(request_context_getter, db_path);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper(
      new AffiliatedMatchHelper(password_store,
                                std::move(affiliation_service)));
  affiliated_match_helper->Initialize();
  password_store->SetAffiliatedMatchHelper(std::move(affiliated_match_helper));

  password_store->enable_propagating_password_changes_to_web_credentials(
      base::FeatureList::IsEnabled(features::kAffiliationBasedMatching));
}

base::FilePath GetAffiliationDatabasePath(const base::FilePath& profile_path) {
  return profile_path.Append(kAffiliationDatabaseFileName);
}

}  // namespace

void ToggleAffiliationBasedMatchingBasedOnPasswordSyncedState(
    PasswordStore* password_store,
    sync_driver::SyncService* sync_service,
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  DCHECK(password_store);

  const bool matching_should_be_active =
      ShouldAffiliationBasedMatchingBeActive(sync_service);
  const bool matching_is_active =
      password_store->affiliated_match_helper() != nullptr;

  if (matching_should_be_active && !matching_is_active) {
    ActivateAffiliationBasedMatching(password_store, request_context_getter,
                                     GetAffiliationDatabasePath(profile_path),
                                     db_thread_runner);
  } else if (!matching_should_be_active && matching_is_active) {
    password_store->SetAffiliatedMatchHelper(
        base::WrapUnique<AffiliatedMatchHelper>(nullptr));
  }
}

void TrimOrDeleteAffiliationCacheForStoreAndPath(
    PasswordStore* password_store,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner) {
  if (password_store && password_store->affiliated_match_helper()) {
    password_store->TrimAffiliationCache();
  } else {
    AffiliationService::DeleteCache(GetAffiliationDatabasePath(profile_path),
                                    db_thread_runner.get());
  }
}

std::unique_ptr<LoginDatabase> CreateLoginDatabase(
    const base::FilePath& profile_path) {
  base::FilePath login_db_file_path = profile_path.Append(kLoginDataFileName);
  return base::WrapUnique(new LoginDatabase(login_db_file_path));
}

}  // namespace password_manager
