// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/change_requests_state_task.h"
#include "components/offline_pages/background/mark_attempt_aborted_task.h"
#include "components/offline_pages/background/mark_attempt_completed_task.h"
#include "components/offline_pages/background/mark_attempt_started_task.h"
#include "components/offline_pages/background/pick_request_task.h"
#include "components/offline_pages/background/pick_request_task_factory.h"
#include "components/offline_pages/background/remove_requests_task.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

namespace {
// Completes the get requests call.
void GetRequestsDone(const RequestQueue::GetRequestsCallback& callback,
                     bool success,
                     std::vector<std::unique_ptr<SavePageRequest>> requests) {
  GetRequestsResult result =
      success ? GetRequestsResult::SUCCESS : GetRequestsResult::STORE_FAILURE;
  // TODO(fgorski): Filter out expired requests based on policy.
  // This may trigger the purging if necessary.
  // Also this may be turned into a method on the request queue or add a policy
  // parameter in the process.
  callback.Run(result, std::move(requests));
}

// Completes the add request call.
void AddRequestDone(const RequestQueue::AddRequestCallback& callback,
                    const SavePageRequest& request,
                    ItemActionStatus status) {
  AddRequestResult result;
  switch (status) {
    case ItemActionStatus::SUCCESS:
      result = AddRequestResult::SUCCESS;
      break;
    case ItemActionStatus::ALREADY_EXISTS:
      result = AddRequestResult::ALREADY_EXISTS;
      break;
    case ItemActionStatus::STORE_ERROR:
      result = AddRequestResult::STORE_FAILURE;
      break;
    case ItemActionStatus::NOT_FOUND:
    default:
      NOTREACHED();
      return;
  }
  callback.Run(result, request);
}

}  // namespace

RequestQueue::RequestQueue(std::unique_ptr<RequestQueueStore> store)
    : store_(std::move(store)), weak_ptr_factory_(this) {}

RequestQueue::~RequestQueue() {}

void RequestQueue::GetRequests(const GetRequestsCallback& callback) {
  store_->GetRequests(base::Bind(&GetRequestsDone, callback));
}

void RequestQueue::AddRequest(const SavePageRequest& request,
                              const AddRequestCallback& callback) {
  // TODO(fgorski): check that request makes sense.
  // TODO(fgorski): check that request does not violate policy.
  store_->AddRequest(request, base::Bind(&AddRequestDone, callback, request));
}

void RequestQueue::RemoveRequests(const std::vector<int64_t>& request_ids,
                                  const UpdateCallback& callback) {
  std::unique_ptr<Task> task(
      new RemoveRequestsTask(store_.get(), request_ids, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::ChangeRequestsState(
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const RequestQueue::UpdateCallback& callback) {
  std::unique_ptr<Task> task(new ChangeRequestsStateTask(
      store_.get(), request_ids, new_state, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::MarkAttemptStarted(int64_t request_id,
                                      const UpdateCallback& callback) {
  std::unique_ptr<Task> task(
      new MarkAttemptStartedTask(store_.get(), request_id, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::MarkAttemptAborted(int64_t request_id,
                                      const UpdateCallback& callback) {
  std::unique_ptr<Task> task(
      new MarkAttemptAbortedTask(store_.get(), request_id, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::MarkAttemptCompleted(int64_t request_id,
                                        const UpdateCallback& callback) {
  std::unique_ptr<Task> task(
      new MarkAttemptCompletedTask(store_.get(), request_id, callback));
  task_queue_.AddTask(std::move(task));
}

void RequestQueue::PurgeRequests(const PurgeRequestsCallback& callback) {}

void RequestQueue::PickNextRequest(
    PickRequestTask::RequestPickedCallback picked_callback,
    PickRequestTask::RequestNotPickedCallback not_picked_callback,
    PickRequestTask::RequestCountCallback request_count_callback,
    DeviceConditions& conditions,
    std::set<int64_t>& disabled_requests) {
  // Using the PickerContext, create a picker task.
  std::unique_ptr<Task> task(picker_factory_->CreatePickerTask(
      store_.get(), picked_callback, not_picked_callback,
      request_count_callback, conditions, disabled_requests));

  // Queue up the picking task, it will call one of the callbacks when it
  // completes.
  task_queue_.AddTask(std::move(task));
}

}  // namespace offline_pages
