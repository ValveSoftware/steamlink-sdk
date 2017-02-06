// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNCABLE_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNCABLE_SERVICE_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "sync/api/syncable_service.h"

namespace autofill {

class AutofillWebDataBackend;
class AutofillWebDataService;

// Syncs masked cards (last 4 digits only) and addresses from the sync user's
// Wallet account.
class AutofillWalletSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService {
 public:
  ~AutofillWalletSyncableService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // Creates a new AutofillWalletSyncableService and hangs it off of
  // |web_data_service|, which takes ownership. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* webdata_backend,
      const std::string& app_locale);

  // Retrieves the AutofillWalletSyncableService stored on |web_data_service|.
  static AutofillWalletSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // Provides a StartSyncFlare to the SyncableService. See
  // sync_start_util for more.
  void InjectStartSyncFlare(
      const syncer::SyncableService::StartSyncFlare& flare);

 protected:
  AutofillWalletSyncableService(
      AutofillWebDataBackend* webdata_backend,
      const std::string& app_locale);

 private:
  syncer::SyncMergeResult SetSyncData(const syncer::SyncDataList& data_list);

  base::ThreadChecker thread_checker_;

  AutofillWebDataBackend* webdata_backend_;  // Weak ref.

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncableService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNCABLE_SERVICE_H_
