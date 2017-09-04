// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/remove_requests_task.h"

#include "base/bind.h"

namespace offline_pages {

RemoveRequestsTask::RemoveRequestsTask(
    RequestQueueStore* store,
    const std::vector<int64_t>& request_ids,
    const RequestQueueStore::UpdateCallback& callback)
    : store_(store),
      request_ids_(request_ids),
      callback_(callback),
      weak_ptr_factory_(this) {}

RemoveRequestsTask::~RemoveRequestsTask() {}

void RemoveRequestsTask::Run() {
  RemoveRequests();
}

void RemoveRequestsTask::RemoveRequests() {
  if (request_ids_.empty()) {
    CompleteEarly(ItemActionStatus::NOT_FOUND);
    return;
  }

  store_->RemoveRequests(request_ids_,
                         base::Bind(&RemoveRequestsTask::CompleteWithResult,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void RemoveRequestsTask::CompleteEarly(ItemActionStatus status) {
  // TODO(fgorski): store_->state() once implemented
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(StoreState::LOADED));
  for (int64_t request_id : request_ids_)
    result->item_statuses.push_back(std::make_pair(request_id, status));
  CompleteWithResult(std::move(result));
}

void RemoveRequestsTask::CompleteWithResult(
    std::unique_ptr<UpdateRequestsResult> result) {
  callback_.Run(std::move(result));
  TaskComplete();
}

}  // namespace offline_pages
