// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/threading/non_thread_safe.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

class FakeServerUpdater;
class ProfileSyncServiceAutofillTest;

namespace syncer {
class SyncErrorFactory;
}

namespace sync_pb {
class AutofillSpecifics;
}

namespace autofill {

class AutofillTable;

// The sync implementation for autocomplete.
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
class AutocompleteSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public AutofillWebDataServiceObserverOnDBThread,
      public base::NonThreadSafe {
 public:
  ~AutocompleteSyncableService() override;

  // Creates a new AutocompleteSyncableService and hangs it off of
  // |web_data_service|, which takes ownership.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* web_data_backend);

  // Retrieves the AutocompleteSyncableService stored on |web_data_service|.
  static AutocompleteSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

  static syncer::ModelType model_type() { return syncer::AUTOFILL; }

  // syncer::SyncableService:
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // AutofillWebDataServiceObserverOnDBThread:
  void AutofillEntriesChanged(const AutofillChangeList& changes) override;

  // Provides a StartSyncFlare to the SyncableService. See sync_start_util for
  // more.
  void InjectStartSyncFlare(
      const syncer::SyncableService::StartSyncFlare& flare);

 protected:
  explicit AutocompleteSyncableService(
      AutofillWebDataBackend* web_data_backend);

  // Helper to query WebDatabase for the current autocomplete state.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool LoadAutofillData(std::vector<AutofillEntry>* entries) const;

  // Helper to persist any changes that occured during model association to
  // the WebDatabase. |entries| will be either added or updated.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool SaveChangesToWebData(const std::vector<AutofillEntry>& entries);

 private:
  friend class ::FakeServerUpdater;
  friend class ::ProfileSyncServiceAutofillTest;

  // This is a helper map used only in Merge/Process* functions. The lifetime
  // of the iterator is longer than the map object. The bool in the pair is used
  // to indicate if the item needs to be added (true) or updated (false).
  typedef std::map<AutofillKey,
                   std::pair<syncer::SyncChange::SyncChangeType,
                             std::vector<AutofillEntry>::iterator>>
      AutocompleteEntryMap;

  // Creates or updates an autocomplete entry based on |data|.
  // |data| - an entry for sync.
  // |loaded_data| - entries that were loaded from local storage.
  // |new_entries| - entries that came from the sync.
  // |ignored_entries| - entries that came from the sync, but too old to be
  // stored and immediately discarded.
  void CreateOrUpdateEntry(const syncer::SyncData& data,
                           AutocompleteEntryMap* loaded_data,
                           std::vector<AutofillEntry>* new_entries);

  // Writes |entry| data into supplied |autofill_specifics|.
  static void WriteAutofillEntry(const AutofillEntry& entry,
                                 sync_pb::EntitySpecifics* autofill_specifics);

  // Deletes the database entry corresponding to the |autofill| specifics.
  syncer::SyncError AutofillEntryDelete(
      const sync_pb::AutofillSpecifics& autofill);

  syncer::SyncData CreateSyncData(const AutofillEntry& entry) const;

  // Syncs |changes| to the cloud.
  void ActOnChanges(const AutofillChangeList& changes);

  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable() const;

  static std::string KeyToTag(const std::string& name,
                              const std::string& value);

  // For unit-tests.
  AutocompleteSyncableService();
  void set_sync_processor(syncer::SyncChangeProcessor* sync_processor) {
    sync_processor_.reset(sync_processor);
  }

  // The |web_data_backend_| is expected to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  ScopedObserver<AutofillWebDataBackend, AutocompleteSyncableService>
      scoped_observer_;

  // We receive ownership of |sync_processor_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // We receive ownership of |error_handler_| in MergeDataAndStartSyncing() and
  // destroy it in StopSyncing().
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteSyncableService);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNCABLE_SERVICE_H_
