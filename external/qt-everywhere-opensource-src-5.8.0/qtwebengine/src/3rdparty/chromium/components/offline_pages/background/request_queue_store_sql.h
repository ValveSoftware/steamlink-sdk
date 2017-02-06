// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_SQL_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_SQL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/request_queue_store.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Connection;
}

namespace offline_pages {

// SQLite implementation of RequestQueueStore.
class RequestQueueStoreSQL : public RequestQueueStore {
 public:
  RequestQueueStoreSQL(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);
  ~RequestQueueStoreSQL() override;

  // RequestQueueStore implementation.
  void GetRequests(const GetRequestsCallback& callback) override;
  void AddOrUpdateRequest(const SavePageRequest& offline_page,
                          const UpdateCallback& callback) override;
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      const RemoveCallback& callback) override;
  void Reset(const ResetCallback& callback) override;

 private:
  // Synchronous implementations, these are run on the background thread
  // and actually do the work to access SQL.  The implementations above
  // simply dispatch to the corresponding *Sync method on the background thread.
  // 'runner' is where to run the callback.
  static void GetRequestsSync(
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const GetRequestsCallback& callback);
  static void AddOrUpdateRequestSync(
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const SavePageRequest& offline_page,
      const UpdateCallback& callback);
  static void RemoveRequestsSync(
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const std::vector<int64_t>& request_ids,
      const RemoveCallback& callback);
  static void ResetSync(sql::Connection* db,
                        const base::FilePath& db_file_path,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const ResetCallback& callback);

  // Used to initialize DB connection.
  static void OpenConnectionSync(
      sql::Connection* db,
      scoped_refptr<base::SingleThreadTaskRunner> runner,
      const base::FilePath& path,
      const base::Callback<void(bool)>& callback);
  void OpenConnection();
  void OnOpenConnectionDone(bool success);

  // Used to finalize connection reset.
  void OnResetDone(const ResetCallback& callback, bool success);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Database connection.
  std::unique_ptr<sql::Connection> db_;

  base::WeakPtrFactory<RequestQueueStoreSQL> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(RequestQueueStoreSQL);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_SQL_H_
