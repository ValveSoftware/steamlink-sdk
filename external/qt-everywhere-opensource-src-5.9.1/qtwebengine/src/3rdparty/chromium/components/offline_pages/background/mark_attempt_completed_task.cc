// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/mark_attempt_completed_task.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"

namespace offline_pages {

MarkAttemptCompletedTask::MarkAttemptCompletedTask(
    RequestQueueStore* store,
    int64_t request_id,
    const RequestQueueStore::UpdateCallback& callback)
    : UpdateRequestTask(store, request_id, callback) {}

MarkAttemptCompletedTask::~MarkAttemptCompletedTask() {}

void MarkAttemptCompletedTask::UpdateRequestImpl(
    std::unique_ptr<UpdateRequestsResult> read_result) {
  if (!ValidateReadResult(read_result.get())) {
    CompleteWithResult(std::move(read_result));
    return;
  }

  // It is perfectly fine to reuse the read_result->updated_items collection, as
  // it is owned by this callback and will be destroyed when out of scope.
  read_result->updated_items[0].MarkAttemptCompleted();
  store()->UpdateRequests(
      read_result->updated_items,
      base::Bind(&MarkAttemptCompletedTask::CompleteWithResult, GetWeakPtr()));
}

}  // namespace offline_pages
