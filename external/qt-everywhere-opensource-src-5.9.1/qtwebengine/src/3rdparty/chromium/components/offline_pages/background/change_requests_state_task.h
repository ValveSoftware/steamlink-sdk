// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class ChangeRequestsStateTask : public Task {
 public:
  ChangeRequestsStateTask(RequestQueueStore* store,
                          const std::vector<int64_t>& request_ids,
                          const SavePageRequest::RequestState new_state,
                          const RequestQueueStore::UpdateCallback& callback);
  ~ChangeRequestsStateTask() override;

  // TaskQueue::Task implementation.
  void Run() override;

 private:
  // Step 1. Reading the requests.
  void ReadRequests();
  // Step 2. Selecting requests to be updated. Calls update.
  void SelectItemsToUpdate(
      bool success,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  // Step 3. Processes update result, calls callback.
  void UpdateCompleted(std::unique_ptr<UpdateRequestsResult> update_result);

  // Completions.
  void CompleteEarly(ItemActionStatus status);
  void CompleteWithStatus(std::unique_ptr<UpdateRequestsResult> result,
                          ItemActionStatus status);

  // Store that this task updates.
  RequestQueueStore* store_;
  // Request IDs to be updated.
  std::unordered_set<int64_t> request_ids_;
  // New state to be set on all entries.
  SavePageRequest::RequestState new_state_;
  // Callback to complete the task.
  RequestQueueStore::UpdateCallback callback_;

  base::WeakPtrFactory<ChangeRequestsStateTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChangeRequestsStateTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_CHANGE_REQUESTS_STATE_TASK_H_
