// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

namespace {
// Completes the get requests call.
void GetRequestsDone(const RequestQueue::GetRequestsCallback& callback,
                     bool success,
                     const std::vector<SavePageRequest>& requests) {
  RequestQueue::GetRequestsResult result =
      success ? RequestQueue::GetRequestsResult::SUCCESS
              : RequestQueue::GetRequestsResult::STORE_FAILURE;
  // TODO(fgorski): Filter out expired requests based on policy.
  // This may trigger the purging if necessary.
  // Also this may be turned into a method on the request queue or add a policy
  // parameter in the process.
  callback.Run(result, requests);
}

// Completes the add request call.
void AddRequestDone(const RequestQueue::AddRequestCallback& callback,
                    const SavePageRequest& request,
                    RequestQueueStore::UpdateStatus status) {
  RequestQueue::AddRequestResult result =
      (status == RequestQueueStore::UpdateStatus::UPDATED)
          ? RequestQueue::AddRequestResult::SUCCESS
          : RequestQueue::AddRequestResult::STORE_FAILURE;
  callback.Run(result, request);
}

// Completes the remove request call.
void RemoveRequestDone(const RequestQueue::UpdateRequestCallback& callback,
                       bool success,
                       int deleted_requests_count) {
  DCHECK_EQ(1, deleted_requests_count);
  RequestQueue::UpdateRequestResult result =
      success ? RequestQueue::UpdateRequestResult::SUCCESS
              : RequestQueue::UpdateRequestResult::STORE_FAILURE;
  callback.Run(result);
}
}  // namespace

RequestQueue::RequestQueue(std::unique_ptr<RequestQueueStore> store)
    : store_(std::move(store)) {}

RequestQueue::~RequestQueue() {}

void RequestQueue::GetRequests(const GetRequestsCallback& callback) {
  store_->GetRequests(base::Bind(&GetRequestsDone, callback));
}

void RequestQueue::AddRequest(const SavePageRequest& request,
                              const AddRequestCallback& callback) {
  // TODO(fgorski): check that request makes sense.
  // TODO(fgorski): check that request does not violate policy.
  store_->AddOrUpdateRequest(request,
                             base::Bind(&AddRequestDone, callback, request));
}

void RequestQueue::UpdateRequest(const SavePageRequest& request,
                                 const UpdateRequestCallback& callback) {}

void RequestQueue::RemoveRequest(int64_t request_id,
                                 const UpdateRequestCallback& callback) {
  std::vector<int64_t> request_ids{request_id};
  store_->RemoveRequests(request_ids, base::Bind(RemoveRequestDone, callback));
}

void RequestQueue::PurgeRequests(const PurgeRequestsCallback& callback) {}

}  // namespace offline_pages
