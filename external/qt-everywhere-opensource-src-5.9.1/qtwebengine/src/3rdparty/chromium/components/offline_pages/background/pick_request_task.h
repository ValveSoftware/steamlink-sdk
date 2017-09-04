// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/request_queue_results.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class DeviceConditions;
class OfflinerPolicy;
class PickRequestTask;
class RequestCoordinatorEventLogger;
class RequestNotifier;
class RequestQueueStore;

typedef bool (PickRequestTask::*RequestCompareFunction)(
    const SavePageRequest* left,
    const SavePageRequest* right);

class PickRequestTask : public Task {
 public:
  // Callback to report when a request was available.
  typedef base::Callback<void(const SavePageRequest& request)>
      RequestPickedCallback;

  // Callback to report when no request was available.
  typedef base::Callback<void(bool)> RequestNotPickedCallback;

  // Callback to report available total and available queued request counts.
  typedef base::Callback<void(size_t, size_t)> RequestCountCallback;

  PickRequestTask(RequestQueueStore* store,
                  OfflinerPolicy* policy,
                  RequestNotifier* notifier,
                  RequestCoordinatorEventLogger* event_logger,
                  RequestPickedCallback picked_callback,
                  RequestNotPickedCallback not_picked_callback,
                  RequestCountCallback request_count_callback,
                  DeviceConditions& device_conditions,
                  const std::set<int64_t>& disabled_requests);

  ~PickRequestTask() override;

  // TaskQueue::Task implementation, starts the async chain
  void Run() override;

 private:
  // Step 1, handle getting the results from the store.
  void ChooseAndPrune(bool request,
                      std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Step 2a. Handle choosing an entry, and calling the right picked callback
  // and the request count callback.
  void ChooseRequestAndCallback(
      std::vector<std::unique_ptr<SavePageRequest>> valid_requests);

  // Step 2b. Handle deleting stale entries and notifying observers.
  void RemoveStaleRequests(std::vector<int64_t> stale_request_ids);

  // Step 3. Send delete notifications for the expired requests.
  void OnRequestsExpired(std::unique_ptr<UpdateRequestsResult> result);

  // Helper functions.

  // Split requests into valid and expired categories.
  void SplitRequests(
      std::vector<std::unique_ptr<SavePageRequest>> requests,
      std::vector<std::unique_ptr<SavePageRequest>>* valid_requests,
      std::vector<int64_t>* expired_request_ids);

  // Determine if this request has device conditions appropriate for running it.
  bool RequestConditionsSatisfied(const SavePageRequest* request);

  // Determine if the new request is preferred under current policies.
  bool IsNewRequestBetter(const SavePageRequest* oldRequest,
                          const SavePageRequest* newRequest,
                          RequestCompareFunction comparator);

  // Returns true if the left hand side is better.
  bool RetryCountFirstCompareFunction(const SavePageRequest* left,
                                      const SavePageRequest* right);

  // Returns true if the left hand side is better.
  bool RecencyFirstCompareFunction(const SavePageRequest* left,
                                   const SavePageRequest* right);

  // Compare left and right side, returning 1 if the left side is better
  // (preferred by policy), 0 if the same, and -1 if the right side is better.
  int CompareRetryCount(const SavePageRequest* left,
                        const SavePageRequest* right);

  // Compare left and right side, returning 1 if the left side is better
  // (preferred by policy), 0 if the same, and -1 if the right side is better.
  int CompareCreationTime(const SavePageRequest* left,
                          const SavePageRequest* right);

  // Member variables, all pointers are not owned here.
  RequestQueueStore* store_;
  OfflinerPolicy* policy_;
  RequestNotifier* notifier_;
  RequestCoordinatorEventLogger* event_logger_;
  RequestPickedCallback picked_callback_;
  RequestNotPickedCallback not_picked_callback_;
  RequestCountCallback request_count_callback_;
  std::unique_ptr<DeviceConditions> device_conditions_;
  const std::set<int64_t>& disabled_requests_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<PickRequestTask> weak_ptr_factory_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_H_
