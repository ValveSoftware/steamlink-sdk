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
class OfflinePageMetadataStoreSQL : public OfflinePageMetadataStore {
 public:
  OfflinePageMetadataStoreSQL(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);
  ~OfflinePageMetadataStoreSQL() override;

  // Implementation methods.
  void Load(const LoadCallback& callback) override;
  void AddOrUpdateOfflinePage(const OfflinePageItem& offline_page,
                              const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64_t>& offline_ids,
                          const UpdateCallback& callback) override;
  void Reset(const ResetCallback& callback) override;

 private:
  // Synchronous implementations, these are run on the background thread
  // and actually do the work to access SQL.  The implementations above
  // simply dispatch to the corresponding *Sync method on the background thread.
  // 'runner' is where to run the callback.
  static void LoadSync(sql::Connection* db,
                       const base::FilePath& path,
                       scoped_refptr<base::SingleThreadTaskRunner> runner,
                       const LoadCallback& callback);
  static void AddOrUpdateOfflinePageSync(
      const OfflinePageItem& offline_page,
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const UpdateCallback& callback);
  static void RemoveOfflinePagesSync(
      const std::vector<int64_t>& offline_ids,
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const UpdateCallback& callback);
  static void ResetSync(std::unique_ptr<sql::Connection> db,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const ResetCallback& callback);

  static void NotifyLoadResult(
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const LoadCallback& callback,
      LoadStatus status,
      const std::vector<OfflinePageItem>& result);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;
  std::unique_ptr<sql::Connection> db_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreSQL);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_SQL_H_
