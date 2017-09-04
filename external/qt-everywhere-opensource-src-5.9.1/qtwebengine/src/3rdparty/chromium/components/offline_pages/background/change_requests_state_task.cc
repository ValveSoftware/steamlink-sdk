// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/change_requests_state_task.h"

#include "base/bind.h"

namespace offline_pages {

ChangeRequestsStateTask::ChangeRequestsStateTask(
    RequestQueueStore* store,
    const std::vector<int64_t>& request_ids,
    const SavePageRequest::RequestState new_state,
    const RequestQueueStore::UpdateCallback& callback)
    : store_(store),
      request_ids_(request_ids.begin(), request_ids.end()),
      new_state_(new_state),
      callback_(callback),
      weak_ptr_factory_(this) {}

ChangeRequestsStateTask::~ChangeRequestsStateTask() {}

void ChangeRequestsStateTask::Run() {
  ReadRequests();
}

void ChangeRequestsStateTask::ReadRequests() {
  if (request_ids_.empty()) {
    CompleteEarly(ItemActionStatus::NOT_FOUND);
    return;
  }

  store_->GetRequests(base::Bind(&ChangeRequestsStateTask::SelectItemsToUpdate,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void ChangeRequestsStateTask::SelectItemsToUpdate(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  if (!success) {
    CompleteEarly(ItemActionStatus::STORE_ERROR);
    return;
  }

  std::vector<SavePageRequest> items_to_update;
  for (const auto& request : requests) {
    // If this request is in our list, update it.
    if (request_ids_.count(request->request_id()) > 0) {
      request->set_request_state(new_state_);
      items_to_update.push_back(*request);
      // Items that are missing before the update will be marked as not found
      // before the callback.
      request_ids_.erase(request->request_id());
    }
  }

  if (items_to_update.empty()) {
    CompleteEarly(ItemActionStatus::NOT_FOUND);
    return;
  }

  store_->UpdateRequests(items_to_update,
                         base::Bind(&ChangeRequestsStateTask::UpdateCompleted,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ChangeRequestsStateTask::UpdateCompleted(
    std::unique_ptr<UpdateRequestsResult> update_result) {
  CompleteWithStatus(std::move(update_result), ItemActionStatus::NOT_FOUND);
}

void ChangeRequestsStateTask::CompleteEarly(ItemActionStatus status) {
  // TODO(fgorski): store_->state() once implemented
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(StoreState::LOADED));
  CompleteWithStatus(std::move(result), status);
}

void ChangeRequestsStateTask::CompleteWithStatus(
    std::unique_ptr<UpdateRequestsResult> result,
    ItemActionStatus status) {
  // Mark items as not found, if they are still in the request IDs set.
  for (int64_t request_id : request_ids_)
    result->item_statuses.push_back(std::make_pair(request_id, status));
  callback_.Run(std::move(result));
  TaskComplete();
}

}  // namespace offline_pages
