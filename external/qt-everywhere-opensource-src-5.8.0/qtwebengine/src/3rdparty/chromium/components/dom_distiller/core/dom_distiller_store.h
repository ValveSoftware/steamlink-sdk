// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/dom_distiller/core/article_attachments_data.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_model.h"
#include "components/dom_distiller/core/dom_distiller_observer.h"
#include "components/leveldb_proto/proto_database.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/syncable_service.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace syncer {
class AttachmentStore;
}

namespace dom_distiller {

// Interface for accessing the stored/synced DomDistiller entries.
class DomDistillerStoreInterface {
 public:
  virtual ~DomDistillerStoreInterface() {}

  // Gets the syncable service for this store or null if it is not synced.
  virtual syncer::SyncableService* GetSyncableService() = 0;

  virtual bool AddEntry(const ArticleEntry& entry) = 0;
  // Returns false if |entry| is not present or |entry| was not updated.
  virtual bool UpdateEntry(const ArticleEntry& entry) = 0;
  virtual bool RemoveEntry(const ArticleEntry& entry) = 0;

  typedef base::Callback<void(bool success)> UpdateAttachmentsCallback;
  typedef base::Callback<
      void(bool success, std::unique_ptr<ArticleAttachmentsData> attachments)>
      GetAttachmentsCallback;

  // Updates the attachments for an entry. The callback will be called with
  // success==true once the new attachments have been stored locally and the
  // entry has been updated. It will be called with success==false if that
  // failed (e.g. storing the attachment failed, the entry couldn't be found,
  // etc.).
  virtual void UpdateAttachments(
      const std::string& entry_id,
      std::unique_ptr<ArticleAttachmentsData> attachments,
      const UpdateAttachmentsCallback& callback) = 0;

  // Gets the attachments for an entry. If the attachments are available (either
  // locally or from sync), the callback will be called with success==true and
  // a pointer to the attachments. Otherwise it will be called with
  // success==false.
  virtual void GetAttachments(const std::string& entry_id,
                              const GetAttachmentsCallback& callback) = 0;

  // Lookup an ArticleEntry by ID or URL. Returns whether a corresponding entry
  // was found. On success, if |entry| is not null, it will contain the entry.
  virtual bool GetEntryById(const std::string& entry_id,
                            ArticleEntry* entry) = 0;
  virtual bool GetEntryByUrl(const GURL& url, ArticleEntry* entry) = 0;

  // Gets a copy of all the current entries.
  virtual std::vector<ArticleEntry> GetEntries() const = 0;

  virtual void AddObserver(DomDistillerObserver* observer) = 0;

  virtual void RemoveObserver(DomDistillerObserver* observer) = 0;
};

// Implements syncing/storing of DomDistiller entries. This keeps three
// models of the DOM distiller data in sync: the local database, sync, and the
// user (i.e. of DomDistillerStore). No changes are accepted while the local
// database is loading. Once the local database has loaded, changes from any of
// the three sources (technically just two, since changes don't come from the
// database) are handled similarly:
// 1. convert the change to a SyncChangeList.
// 2. apply that change to the in-memory model, calculating what changed
// (changes_applied) and what is missing--i.e. entries missing for a full merge,
// conflict resolution for normal changes-- (changes_missing).
// 3. send a message (possibly handled asynchronously) containing
// changes_missing to the source of the change.
// 4. send messages (possibly handled asynchronously) containing changes_applied
// to the other (i.e. non-source) two models.
// TODO(cjhopman): Support deleting entries.
class DomDistillerStore : public syncer::SyncableService,
                          public DomDistillerStoreInterface {
 public:
  typedef std::vector<ArticleEntry> EntryVector;

  // Creates storage using the given database for local storage. Initializes the
  // database with |database_dir|.
  DomDistillerStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<ArticleEntry>> database,
      const base::FilePath& database_dir);

  // Creates storage using the given database for local storage. Initializes the
  // database with |database_dir|.  Also initializes the internal model to
  // |initial_model|.
  DomDistillerStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<ArticleEntry>> database,
      const std::vector<ArticleEntry>& initial_data,
      const base::FilePath& database_dir);

  ~DomDistillerStore() override;

  // DomDistillerStoreInterface implementation.
  syncer::SyncableService* GetSyncableService() override;

  bool AddEntry(const ArticleEntry& entry) override;
  bool UpdateEntry(const ArticleEntry& entry) override;
  bool RemoveEntry(const ArticleEntry& entry) override;

  void UpdateAttachments(
      const std::string& entry_id,
      std::unique_ptr<ArticleAttachmentsData> attachments_data,
      const UpdateAttachmentsCallback& callback) override;
  void GetAttachments(const std::string& entry_id,
                      const GetAttachmentsCallback& callback) override;

  bool GetEntryById(const std::string& entry_id, ArticleEntry* entry) override;
  bool GetEntryByUrl(const GURL& url, ArticleEntry* entry) override;
  std::vector<ArticleEntry> GetEntries() const override;

  void AddObserver(DomDistillerObserver* observer) override;
  void RemoveObserver(DomDistillerObserver* observer) override;

  // syncer::SyncableService implementation.
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

 private:
  void OnDatabaseInit(bool success);
  void OnDatabaseLoad(bool success, std::unique_ptr<EntryVector> entries);
  void OnDatabaseSave(bool success);

  // Returns true if the change is successfully applied.
  bool ChangeEntry(const ArticleEntry& entry,
                   syncer::SyncChange::SyncChangeType changeType);

  void OnAttachmentsWrite(
      const std::string& entry_id,
      std::unique_ptr<sync_pb::ArticleAttachments> article_attachments,
      const UpdateAttachmentsCallback& callback,
      const syncer::AttachmentStore::Result& result);

  void OnAttachmentsRead(const sync_pb::ArticleAttachments& attachments_proto,
                         const GetAttachmentsCallback& callback,
                         const syncer::AttachmentStore::Result& result,
                         std::unique_ptr<syncer::AttachmentMap> attachments,
                         std::unique_ptr<syncer::AttachmentIdList> missing);

  syncer::SyncMergeResult MergeDataWithModel(
      const syncer::SyncDataList& data, syncer::SyncChangeList* changes_applied,
      syncer::SyncChangeList* changes_missing);

  // Convert a SyncDataList to a SyncChangeList of add or update changes based
  // on the state of the in-memory model. Also calculate the entries missing
  // from the SyncDataList.
  void CalculateChangesForMerge(const syncer::SyncDataList& data,
                                syncer::SyncChangeList* changes_to_apply,
                                syncer::SyncChangeList* changes_missing);

  bool ApplyChangesToSync(const tracked_objects::Location& from_here,
                          const syncer::SyncChangeList& change_list);
  bool ApplyChangesToDatabase(const syncer::SyncChangeList& change_list);

  // Applies the changes to |model_|. If the model returns an error, disables
  // syncing and database changes and returns false.
  void ApplyChangesToModel(const syncer::SyncChangeList& change_list,
                           syncer::SyncChangeList* changes_applied,
                           syncer::SyncChangeList* changes_missing);

  void NotifyObservers(const syncer::SyncChangeList& changes);

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_factory_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<ArticleEntry>> database_;
  bool database_loaded_;
  std::unique_ptr<syncer::AttachmentStore> attachment_store_;
  base::ObserverList<DomDistillerObserver> observers_;

  DomDistillerModel model_;

  base::WeakPtrFactory<DomDistillerStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerStore);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_
