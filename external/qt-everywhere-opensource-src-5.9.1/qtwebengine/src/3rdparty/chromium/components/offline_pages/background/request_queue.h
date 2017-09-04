// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/pick_request_task.h"
#include "components/offline_pages/background/pick_request_task_factory.h"
#include "components/offline_pages/background/request_queue_results.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/core/task_queue.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_store_types.h"

namespace offline_pages {

class RequestQueueStore;
class PickRequestTaskFactory;

// Class responsible for managing save page requests.
class RequestQueue {
 public:

  // Callback used for |GetRequests|.
  typedef base::Callback<void(GetRequestsResult,
                              std::vector<std::unique_ptr<SavePageRequest>>)>
      GetRequestsCallback;

  // Callback used for |AddRequest|.
  typedef base::Callback<void(AddRequestResult, const SavePageRequest& request)>
      AddRequestCallback;

  // Callback used by |ChangeRequestsState|.
  typedef base::Callback<void(std::unique_ptr<UpdateRequestsResult>)>
      UpdateCallback;

  // Callback used by |UdpateRequest|.
  typedef base::Callback<void(UpdateRequestResult)> UpdateRequestCallback;

  explicit RequestQueue(std::unique_ptr<RequestQueueStore> store);
  ~RequestQueue();

  // Gets all of the active requests from the store. Calling this method may
  // schedule purging of the request queue.
  void GetRequests(const GetRequestsCallback& callback);

  // Adds |request| to the request queue. Result is returned through |callback|.
  // In case adding the request violates policy, the result will fail with
  // appropriate result. Callback will also return a copy of a request with all
  // fields set.
  void AddRequest(const SavePageRequest& request,
                  const AddRequestCallback& callback);

  // Removes the requests matching the |request_ids|. Result is returned through
  // |callback|.  If a request id cannot be removed, this will still remove the
  // others.
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      const UpdateCallback& callback);

  // Changes the state to |new_state| for requests matching the
  // |request_ids|. Results are returned through |callback|.
  void ChangeRequestsState(const std::vector<int64_t>& request_ids,
                           const SavePageRequest::RequestState new_state,
                           const UpdateCallback& callback);

  // Marks attempt with |request_id| as started. Results are returned through
  // |callback|.
  void MarkAttemptStarted(int64_t request_id, const UpdateCallback& callback);

  // Marks attempt with |request_id| as aborted. Results are returned through
  // |callback|.
  void MarkAttemptAborted(int64_t request_id, const UpdateCallback& callback);

  // Marks attempt with |request_id| as completed. The attempt may have
  // completed with either success or failure (not denoted here). Results
  // are returned through |callback|.
  void MarkAttemptCompleted(int64_t request_id, const UpdateCallback& callback);

  // Make a task to pick the next request, and report our choice to the
  // callbacks.
  void PickNextRequest(
      PickRequestTask::RequestPickedCallback picked_callback,
      PickRequestTask::RequestNotPickedCallback not_picked_callback,
      PickRequestTask::RequestCountCallback request_count_callback,
      DeviceConditions& conditions,
      std::set<int64_t>& disabled_requests);

  // Takes ownership of the factory.  We use a setter to allow users of the
  // request queue to not need a PickerFactory to create it, since we have lots
  // of code using the request queue.  The request coordinator will set a
  // factory before calling PickNextRequest.
  void SetPickerFactory(std::unique_ptr<PickRequestTaskFactory> factory) {
    picker_factory_ = std::move(factory);
  }

 private:
  // Callback used by |PurgeRequests|.
  typedef base::Callback<void(UpdateRequestResult,
                              int /* removed requests count */)>
      PurgeRequestsCallback;

  // Purges the queue, removing the requests that are no longer relevant, e.g.
  // expired request. Result is returned through |callback| carries the number
  // of removed requests.
  void PurgeRequests(const PurgeRequestsCallback& callback);

  std::unique_ptr<RequestQueueStore> store_;

  // Task queue to serialize store access.
  TaskQueue task_queue_;

  // Builds PickRequestTask objects.
  std::unique_ptr<PickRequestTaskFactory> picker_factory_;

  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestQueue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RequestQueue);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_H_
