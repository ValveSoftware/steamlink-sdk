// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/offline_pages/archive_manager.h"
#include "components/offline_pages/offline_page_types.h"

namespace base {
class Clock;
}  // namespace base

namespace offline_pages {

// Maximum % of total available storage that will be occupied by offline pages
// before a storage clearup.
const double kOfflinePageStorageLimit = 0.3;
// The target % of storage usage we try to reach below when expiring pages.
const double kOfflinePageStorageClearThreshold = 0.1;
// The time that the storage cleanup will be triggered again since the last one.
const base::TimeDelta kClearStorageInterval = base::TimeDelta::FromMinutes(10);
// The time that the page record will be removed from the store since the page
// has been expired.
const base::TimeDelta kRemovePageItemInterval = base::TimeDelta::FromDays(21);

class ClientPolicyController;
class OfflinePageModel;

// This class is used for storage management of offline pages. It provides
// a ClearPagesIfNeeded method which is used to clear expired offline pages
// based on last_access_time and lifetime policy of its namespace.
// It has its own throttle mechanism so calling the method would not be
// guaranteed to clear the pages immediately.
//
// OfflinePageModel should own and control the lifecycle of this manager.
// And this manager would use OfflinePageModel to get/remove pages.
class OfflinePageStorageManager {
 public:
  enum class ClearStorageResult {
    SUCCESS,                     // Cleared successfully.
    UNNECESSARY,                 // No expired pages.
    EXPIRE_FAILURE,              // Expiration failed.
    DELETE_FAILURE,              // Deletion failed.
    EXPIRE_AND_DELETE_FAILURES,  // Both expiration and deletion failed.
    // NOTE: always keep this entry at the end. Add new result types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    RESULT_COUNT,
  };

  // Callback used when calling ClearPagesIfNeeded.
  // size_t: the number of expired pages.
  // ClearStorageResult: result of expiring pages in storage.
  typedef base::Callback<void(size_t, ClearStorageResult)> ClearStorageCallback;

  explicit OfflinePageStorageManager(OfflinePageModel* model,
                                     ClientPolicyController* policy_controller,
                                     ArchiveManager* archive_manager);

  ~OfflinePageStorageManager();

  // The manager would *try* to clear pages when called. It may not delete any
  // pages (if clearing condition wasn't satisfied).
  // It clears the storage (expire pages) when it's using more disk space than a
  // certain limit, or the time elapsed from last time clearing is longer than a
  // certain interval. Both values are defined above.
  void ClearPagesIfNeeded(const ClearStorageCallback& callback);

  // Sets the clock for testing.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  // Enum indicating how to clear the pages.
  enum class ClearMode {
    // Using normal expiration logic to expire pages. Will reduce the storage
    // usage down below the threshold.
    DEFAULT,
    // No need to expire any page (no pages in the model or no expired
    // pages and we're not exceeding the storage limit.)
    NOT_NEEDED,
  };

  // Callback called after getting storage stats from archive manager.
  void OnGetStorageStatsDoneForClearingPages(
      const ClearStorageCallback& callback,
      const ArchiveManager::StorageStats& pages);

  // Callback called after getting all pages from model.
  void OnGetAllPagesDoneForClearingPages(
      const ClearStorageCallback& callback,
      const ArchiveManager::StorageStats& storage_stats,
      const MultipleOfflinePageItemResult& pages);

  // Callback called after expired pages have been deleted.
  void OnPagesExpired(const ClearStorageCallback& callback,
                      size_t pages_to_clear,
                      const std::vector<int64_t>& page_ids_to_remove,
                      bool expiration_succeeded);

  // Callback called after clearing outdated pages from model.
  void OnOutdatedPagesCleared(const ClearStorageCallback& callback,
                              size_t pages_cleared,
                              bool expiration_succeeded,
                              DeletePageResult result);

  // Gets offline IDs of both pages that should be expired and the ones that
  // need to be removed from metadata store. |page_ids_to_expire| will have
  // the pages to be expired, |page_ids_to_remove| will have the pages to be
  // removed.
  void GetPageIdsToClear(const MultipleOfflinePageItemResult& pages,
                         const ArchiveManager::StorageStats& stats,
                         std::vector<int64_t>* page_ids_to_expire,
                         std::vector<int64_t>* page_ids_to_remove);

  // Determines if manager should clear pages.
  ClearMode ShouldClearPages(const ArchiveManager::StorageStats& storage_stats);

  // Returns true if |page| is expired comparing to |clear_time_|.
  bool ShouldBeExpired(const OfflinePageItem& page) const;

  // Returns true if we're currently doing a cleanup.
  bool IsInProgress() const;

  // Not owned.
  OfflinePageModel* model_;

  // Not owned.
  ClientPolicyController* policy_controller_;

  // Not owned.
  ArchiveManager* archive_manager_;

  // Starting time of the current storage cleanup. If this time is later than
  // |last_clear_time_| it means we're doing a cleanup.
  base::Time clear_time_;

  // Timestamp of last storage cleanup.
  base::Time last_clear_time_;

  // Clock for getting time.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<OfflinePageStorageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageStorageManager);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_STORAGE_MANAGER_H_
