// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNCABLE_SERVICE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNCABLE_SERVICE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/history_types.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace base {
class MessageLoop;
};

namespace sync_pb {
class TypedUrlSpecifics;
};

namespace history {

class HistoryBackend;
class TypedUrlSyncableServiceTest;
class URLRow;

class TypedUrlSyncableService : public syncer::SyncableService,
                                public history::HistoryBackendObserver {
 public:
  explicit TypedUrlSyncableService(HistoryBackend* history_backend);
  ~TypedUrlSyncableService() override;

  static syncer::ModelType model_type() { return syncer::TYPED_URLS; }

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

  // history::HistoryBackendObserver:
  void OnURLVisited(history::HistoryBackend* history_backend,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsModified(history::HistoryBackend* history_backend,
                      const history::URLRows& changed_urls) override;
  void OnURLsDeleted(history::HistoryBackend* history_backend,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Returns the percentage of DB accesses that have resulted in an error.
  int GetErrorPercentage() const;

  // Converts the passed URL information to a TypedUrlSpecifics structure for
  // writing to the sync DB.
  static void WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       sync_pb::TypedUrlSpecifics* specifics);

 private:
  friend class TypedUrlSyncableServiceTest;

  typedef std::vector<std::pair<URLID, URLRow>> TypedUrlUpdateVector;
  typedef std::vector<std::pair<GURL, std::vector<VisitInfo>>>
      TypedUrlVisitVector;

  // This is a helper map used only in Merge/Process* functions. The lifetime
  // of the iterator is longer than the map object.
  typedef std::map<GURL, std::pair<syncer::SyncChange::SyncChangeType, URLRow>>
      TypedUrlMap;

  // This is a helper map used to associate visit vectors from the history db
  // to the typed urls in the above map.
  typedef std::map<GURL, VisitVector> UrlVisitVectorMap;

  // Bitfield returned from MergeUrls to specify the result of a merge.
  typedef uint32_t MergeResult;
  static const MergeResult DIFF_NONE = 0;
  static const MergeResult DIFF_UPDATE_NODE = 1 << 0;
  static const MergeResult DIFF_LOCAL_ROW_CHANGED = 1 << 1;
  static const MergeResult DIFF_LOCAL_VISITS_ADDED = 1 << 2;

  // Helper method for getting the set of synced urls.
  // Set it as virtual for testing.
  void GetSyncedUrls(std::set<GURL>* urls) const;

  // Helper function that clears our error counters (used to reset stats after
  // model association so we can track model association errors separately).
  virtual void ClearErrorStats();

  // Compares |typed_url| from the server against local history to decide how
  // to merge any existing data, and updates appropriate data containers to
  // write to server and backend.
  void CreateOrUpdateUrl(const sync_pb::TypedUrlSpecifics& typed_url,
                         TypedUrlMap* loaded_data,
                         UrlVisitVectorMap* visit_vectors,
                         history::URLRows* new_synced_urls,
                         TypedUrlVisitVector* new_synced_visits,
                         history::URLRows* updated_synced_urls);

  // Given a TypedUrlSpecifics object, removes all visits that are older than
  // the current expiration time. Note that this can result in having no visits
  // at all.
  sync_pb::TypedUrlSpecifics FilterExpiredVisits(
      const sync_pb::TypedUrlSpecifics& specifics);

  // Merges the URL information in |typed_url| with the URL information from the
  // history database in |url| and |visits|, and returns a bitmask with the
  // results of the merge:
  // DIFF_UPDATE_NODE - changes have been made to |new_url| and |visits| which
  //   should be persisted to the sync node.
  // DIFF_LOCAL_ROW_CHANGED - The history data in |new_url| should be persisted
  //   to the history DB.
  // DIFF_LOCAL_VISITS_ADDED - |new_visits| contains a list of visits that
  //   should be written to the history DB for this URL. Deletions are not
  //   written to the DB - each client is left to age out visits on their own.
  static MergeResult MergeUrls(const sync_pb::TypedUrlSpecifics& typed_url,
                               const history::URLRow& url,
                               history::VisitVector* visits,
                               history::URLRow* new_url,
                               std::vector<history::VisitInfo>* new_visits);

  // Writes new typed url data from sync server to history backend.
  void WriteToHistoryBackend(const history::URLRows* new_urls,
                             const history::URLRows* updated_urls,
                             const std::vector<GURL>* deleted_urls,
                             const TypedUrlVisitVector* new_visits,
                             const history::VisitVector* deleted_visits);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, because it contains invalid data.
  bool ShouldIgnoreUrl(const GURL& url);

  // Helper function that determines if we should ignore a URL for the purposes
  // of sync, based on the visits the URL had.
  bool ShouldIgnoreVisits(const history::VisitVector& visits);

  // Returns true if the caller should sync as a result of the passed visit
  // notification. We use this to throttle the number of sync changes we send
  // to the server so we don't hit the server for every
  // single typed URL visit.
  bool ShouldSyncVisit(int typed_count, ui::PageTransition transition);

  // Utility routine that either updates an existing sync node or creates a
  // new one for the passed |typed_url| if one does not already exist. Returns
  // false and sets an unrecoverable error if the operation failed.
  bool CreateOrUpdateSyncNode(URLRow typed_url,
                              syncer::SyncChangeList* changes);

  // Utility routine for building a change from a typed url.
  void AddTypedUrlToChangeList(syncer::SyncChange::SyncChangeType change_type,
                               const history::URLRow& row,
                               const history::VisitVector& visits,
                               std::string title,
                               syncer::SyncChangeList* change_list);

  // Fills |new_url| with formatted data from |typed_url|.
  static void UpdateURLRowFromTypedUrlSpecifics(
      const sync_pb::TypedUrlSpecifics& typed_url,
      history::URLRow* new_url);

  // Fetches visits from the history DB corresponding to the passed URL. This
  // function compensates for the fact that the history DB has rather poor data
  // integrity (duplicate visits, visit timestamps that don't match the
  // last_visit timestamp, huge data sets that exhaust memory when fetched,
  // etc) by modifying the passed |url| object and |visits| vector.
  // Returns false if we could not fetch the visits for the passed URL, and
  // tracks DB error statistics internally for reporting via UMA.
  virtual bool FixupURLAndGetVisits(URLRow* url, VisitVector* visits);

  // Given a typed URL in the sync DB, looks for an existing entry in the
  // local history DB and generates a list of visits to add to the
  // history DB to bring it up to date (avoiding duplicates).
  // Updates the passed |visits_to_add| and |visits_to_remove| vectors with the
  // visits to add to/remove from the history DB, and adds a new entry to either
  // |updated_urls| or |new_urls| depending on whether the URL already existed
  // in the history DB.
  void UpdateFromSyncDB(const sync_pb::TypedUrlSpecifics& typed_url,
                        TypedUrlVisitVector* visits_to_add,
                        history::VisitVector* visits_to_remove,
                        history::URLRows* updated_urls,
                        history::URLRows* new_urls);

  // Diffs the set of visits between the history DB and the sync DB, using the
  // sync DB as the canonical copy. Result is the set of |new_visits| and
  // |removed_visits| that can be applied to the history DB to make it match
  // the sync DB version. |removed_visits| can be null if the caller does not
  // care about which visits to remove.
  static void DiffVisits(const history::VisitVector& history_visits,
                         const sync_pb::TypedUrlSpecifics& sync_specifics,
                         std::vector<history::VisitInfo>* new_visits,
                         history::VisitVector* removed_visits);

  // TODO(sync): Consider using "delete all" sync logic instead of in-memory
  // cache of typed urls. See http://crbug.com/231689.
  std::set<GURL> synced_typed_urls_;

  // The backend we're syncing local changes from and sync changes to.
  HistoryBackend* const history_backend_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local url changes, since we triggered them.
  bool processing_syncer_changes_;

  // We receive ownership of |sync_processor_| and |error_handler_| in
  // MergeDataAndStartSyncing() and destroy them in StopSyncing().
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_handler_;

  // Statistics for the purposes of tracking the percentage of DB accesses that
  // fail for each client via UMA.
  int num_db_accesses_;
  int num_db_errors_;

  base::ThreadChecker thread_checker_;

  ScopedObserver<history::HistoryBackend, history::HistoryBackendObserver>
      history_backend_observer_;

  DISALLOW_COPY_AND_ASSIGN(TypedUrlSyncableService);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TYPED_URL_SYNCABLE_SERVICE_H_
