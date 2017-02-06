// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_archiver.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_model_event_logger.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "components/offline_pages/offline_page_types.h"

class GURL;
namespace base {
class SequencedTaskRunner;
class Time;
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace offline_pages {

static const int64_t kInvalidOfflineId = 0;

struct ClientId;
struct OfflinePageItem;

class ArchiveManager;
class ClientPolicyController;
class OfflinePageMetadataStore;
class OfflinePageStorageManager;

// Implementation of service for saving pages offline, storing the offline
// copy and metadata, and retrieving them upon request.
class OfflinePageModelImpl : public OfflinePageModel, public KeyedService {
 public:
  // All blocking calls/disk access will happen on the provided |task_runner|.
  OfflinePageModelImpl(
      std::unique_ptr<OfflinePageMetadataStore> store,
      const base::FilePath& archives_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~OfflinePageModelImpl() override;

  // Implemented methods:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const GURL& url,
                const ClientId& client_id,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void ClearAll(const base::Closure& callback) override;
  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;
  void DeletePagesByURLPredicate(const UrlPredicate& predicate,
                                 const DeletePageCallback& callback) override;
  void HasPages(const std::string& name_space,
                const HasPagesCallback& callback) override;
  void CheckPagesExistOffline(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback) override;
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override;
  void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) override;
  const std::vector<int64_t> MaybeGetOfflineIdsForClientId(
      const ClientId& client_id) const override;
  void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) override;
  const OfflinePageItem* MaybeGetPageByOfflineId(
      int64_t offline_id) const override;
  void GetPageByOfflineURL(
      const GURL& offline_url,
      const SingleOfflinePageItemCallback& callback) override;
  const OfflinePageItem* MaybeGetPageByOfflineURL(
      const GURL& offline_url) const override;
  void GetPagesByOnlineURL(
      const GURL& online_url,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetBestPageForOnlineURL(
      const GURL& online_url,
      const SingleOfflinePageItemCallback callback) override;
  const OfflinePageItem* MaybeGetBestPageForOnlineURL(
      const GURL& online_url) const override;
  void CheckMetadataConsistency() override;
  void ExpirePages(const std::vector<int64_t>& offline_ids,
                   const base::Time& expiration_time,
                   const base::Callback<void(bool)>& callback) override;
  ClientPolicyController* GetPolicyController() override;

  // Methods for testing only:
  OfflinePageMetadataStore* GetStoreForTesting();

  OfflinePageStorageManager* GetStorageManager();

  bool is_loaded() const override;

  OfflineEventLogger* GetLogger() override;

 protected:
  // Adding a protected constructor for testing-only purposes in
  // offline_page_storage_manager_unittest.cc
  OfflinePageModelImpl();

 private:
  FRIEND_TEST_ALL_PREFIXES(OfflinePageModelImplTest, MarkPageForDeletion);

  typedef ScopedVector<OfflinePageArchiver> PendingArchivers;

  // Callback for ensuring archive directory is created.
  void OnEnsureArchivesDirCreatedDone(const base::TimeTicks& start_time);

  void GetAllPagesAfterLoadDone(
      const MultipleOfflinePageItemCallback& callback) const;
  void CheckPagesExistOfflineAfterLoadDone(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback);
  void GetOfflineIdsForClientIdWhenLoadDone(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) const;
  void GetPageByOfflineIdWhenLoadDone(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) const;
  void GetPagesByOnlineURLWhenLoadDone(
      const GURL& offline_url,
      const MultipleOfflinePageItemCallback& callback) const;
  void GetPageByOfflineURLWhenLoadDone(
      const GURL& offline_url,
      const SingleOfflinePageItemCallback& callback) const;
  void GetBestPageForOnlineURLWhenLoadDone(
      const GURL& online_url,
      const SingleOfflinePageItemCallback& callback) const;
  void MarkPageAccessedWhenLoadDone(int64_t offline_id);
  void CheckMetadataConsistencyWhenLoadDone();

  // Callback for checking whether we have offline pages.
  void HasPagesAfterLoadDone(const std::string& name_space,
                             const HasPagesCallback& callback) const;

  // Callback for loading pages from the offline page metadata store.
  void OnLoadDone(const base::TimeTicks& start_time,
                  OfflinePageMetadataStore::LoadStatus load_status,
                  const std::vector<OfflinePageItem>& offline_pages);

  // Steps for saving a page offline.
  void OnCreateArchiveDone(const GURL& requested_url,
                           int64_t offline_id,
                           const ClientId& client_id,
                           const base::Time& start_time,
                           const SavePageCallback& callback,
                           OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           int64_t file_size);
  void OnAddOfflinePageDone(OfflinePageArchiver* archiver,
                            const SavePageCallback& callback,
                            const OfflinePageItem& offline_page,
                            bool success);
  void InformSavePageDone(const SavePageCallback& callback,
                          SavePageResult result,
                          const ClientId& client_id,
                          int64_t offline_id);
  void DeletePendingArchiver(OfflinePageArchiver* archiver);

  // Steps for deleting files and data for an offline page.
  void OnDeleteArchiveFilesDone(const std::vector<int64_t>& offline_ids,
                                const DeletePageCallback& callback,
                                bool success);
  void OnRemoveOfflinePagesDone(const std::vector<int64_t>& offline_ids,
                                const DeletePageCallback& callback,
                                bool success);
  void InformDeletePageDone(const DeletePageCallback& callback,
                            DeletePageResult result);

  void OnMarkPageAccesseDone(const OfflinePageItem& offline_page_item,
                             bool success);

  // Callbacks for checking metadata consistency.
  void CheckMetadataConsistencyForArchivePaths(
      const std::set<base::FilePath>& archive_paths);
  // Callback called after headless archives deleted. Orphaned archives are
  // archives files on disk which are not pointed to by any of the page items
  // in metadata store.
  void ExpirePagesMissingArchiveFile(
      const std::set<base::FilePath>& archive_paths);
  void OnExpirePagesMissingArchiveFileDone(
      const std::vector<int64_t>& offline_ids,
      bool success);
  void DeleteOrphanedArchives(const std::set<base::FilePath>& archive_paths);
  void OnDeleteOrphanedArchivesDone(const std::vector<base::FilePath>& archives,
                                    bool success);

  // Callbacks for deleting pages with same URL when saving pages.
  void OnPagesFoundWithSameURL(const ClientId& client_id,
                               int64_t offline_id,
                               size_t pages_allowed,
                               const MultipleOfflinePageItemResult& items);
  void OnDeleteOldPagesWithSameURL(DeletePageResult result);

  // Steps for clearing all.
  void OnRemoveAllFilesDoneForClearAll(const base::Closure& callback,
                                       DeletePageResult result);
  void OnResetStoreDoneForClearAll(const base::Closure& callback, bool success);
  void OnReloadStoreDoneForClearAll(
      const base::Closure& callback,
      OfflinePageMetadataStore::LoadStatus load_status,
      const std::vector<OfflinePageItem>& offline_pages);

  void CacheLoadedData(const std::vector<OfflinePageItem>& offline_pages);

  // Actually does the work of deleting, requires the model is loaded.
  void DoDeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                                const DeletePageCallback& callback);

  // Similar to DoDeletePagesByOfflineId, does actual work of deleting, and
  // requires that the model is loaded.
  void DoDeletePagesByURLPredicate(const UrlPredicate& predicate,
                                   const DeletePageCallback& callback);

  // Callback completing page expiration.
  void OnExpirePageDone(int64_t offline_id,
                        const base::Time& expiration_time,
                        bool success);

  // Clears expired pages if there are any.
  void ClearStorageIfNeeded(
      const OfflinePageStorageManager::ClearStorageCallback& callback);

  // Callback completing storage clearing.
  void OnStorageCleared(size_t expired_page_count,
                        OfflinePageStorageManager::ClearStorageResult result);

  // Post task to clear storage.
  void PostClearStorageIfNeededTask();

  void RunWhenLoaded(const base::Closure& job);

  // Persistent store for offline page metadata.
  std::unique_ptr<OfflinePageMetadataStore> store_;

  // Location where all of the archive files will be stored.
  base::FilePath archives_dir_;

  // The observers.
  base::ObserverList<Observer> observers_;

  bool is_loaded_;

  // In memory copy of the offline page metadata, keyed by bookmark IDs.
  std::map<int64_t, OfflinePageItem> offline_pages_;

  // Pending archivers owned by this model.
  PendingArchivers pending_archivers_;

  // Delayed tasks that should be invoked after the loading is done.
  std::vector<base::Closure> delayed_tasks_;

  // Controller of the client policies.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  // Manager for the storage consumed by archives and responsible for
  // automatic page clearing.
  std::unique_ptr<OfflinePageStorageManager> storage_manager_;

  // Manager for the offline archive files and directory.
  std::unique_ptr<ArchiveManager> archive_manager_;

  // Logger to facilitate recording of events.
  OfflinePageModelEventLogger offline_event_logger_;

  base::WeakPtrFactory<OfflinePageModelImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_IMPL_H_
