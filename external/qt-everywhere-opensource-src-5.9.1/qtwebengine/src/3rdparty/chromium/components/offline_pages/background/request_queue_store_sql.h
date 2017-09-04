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
  // Note: current implementation of this method makes a SQL query per ID. This
  // is OK as long as number of IDs stays low, which is a typical case.
  // Implementation should be revisited in case that presumption changes.
  void GetRequestsByIds(const std::vector<int64_t>& request_ids,
                        const UpdateCallback& callback) override;
  void AddRequest(const SavePageRequest& offline_page,
                  const AddCallback& callback) override;
  void UpdateRequests(const std::vector<SavePageRequest>& requests,
                      const UpdateCallback& callback) override;
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      const UpdateCallback& callback) override;
  void Reset(const ResetCallback& callback) override;
  StoreState state() const override;

 private:
  // Helper functions to return immediately if no database is found.
  bool CheckDb(const base::Closure& callback);

  // Used to initialize DB connection.
  void OpenConnection();
  void OnOpenConnectionDone(StoreState state);

  // Used to finalize connection reset.
  void OnResetDone(const ResetCallback& callback, StoreState state);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Database connection.
  std::unique_ptr<sql::Connection> db_;

  // State of the store.
  StoreState state_;

  base::WeakPtrFactory<RequestQueueStoreSQL> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(RequestQueueStoreSQL);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_SQL_H_
