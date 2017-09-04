// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_SQL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_SQL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/offline_page_metadata_store.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Connection;
}

namespace offline_pages {

// OfflinePageMetadataStoreSQL is an instance of OfflinePageMetadataStore
// which is implemented using a SQLite database.
//
// This store has a history of schema updates in pretty much every release.
// Original schema was delivered in M52. Since then, the following changes
// happened:
// * In M53 expiration_time was added,
// * In M54 title was added,
// * In M55 we dropped the following fields (never used): version, status,
//   offline_url, user_initiated.
// * In M56 original_url was added.
//
// Here is a procedure to update the schema for this store:
// * Decide how to detect that the store is on a particular version, which
//   typically means that a certain field exists or is missing. This happens in
//   Upgrade section of |CreateSchema|
// * Work out appropriate change and apply it to all existing upgrade paths. In
//   the interest of performing a single update of the store, it upgrades from a
//   detected version to the current one. This means that when making a change,
//   more than a single query may have to be updated (in case of fields being
//   removed or needed to be initialized to a specific, non-default value).
//   Such approach is preferred to doing N updates for every changed version on
//   a startup after browser update.
// * New upgrade method should specify which version it is upgrading from, e.g.
//   |UpgradeFrom54|.
// * Upgrade should use |UpgradeWithQuery| and simply specify SQL command to
//   move data from old table (prefixed by temp_) to the new one.
class OfflinePageMetadataStoreSQL : public OfflinePageMetadataStore {
 public:
  OfflinePageMetadataStoreSQL(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);
  ~OfflinePageMetadataStoreSQL() override;

  // Implementation methods.
  void Initialize(const InitializeCallback& callback) override;
  void GetOfflinePages(const LoadCallback& callback) override;
  void AddOfflinePage(const OfflinePageItem& offline_page,
                      const AddCallback& callback) override;
  void UpdateOfflinePages(const std::vector<OfflinePageItem>& pages,
                          const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64_t>& offline_ids,
                          const UpdateCallback& callback) override;
  void Reset(const ResetCallback& callback) override;
  StoreState state() const override;

  // Helper function used to force incorrect state for testing purposes.
  void SetStateForTesting(StoreState state, bool reset_db);

 private:
  // Used to conclude opening/resetting DB connection.
  void OnOpenConnectionDone(const InitializeCallback& callback, bool success);
  void OnResetDone(const ResetCallback& callback, bool success);

  // Checks whether a valid DB connection is present and store state is LOADED.
  bool CheckDb();

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Database connection.
  std::unique_ptr<sql::Connection> db_;

  // State of the store.
  StoreState state_;

  base::WeakPtrFactory<OfflinePageMetadataStoreSQL> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreSQL);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_SQL_H_
