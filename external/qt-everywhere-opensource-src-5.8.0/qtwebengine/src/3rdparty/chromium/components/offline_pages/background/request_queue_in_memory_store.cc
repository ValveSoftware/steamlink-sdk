// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_in_memory_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

RequestQueueInMemoryStore::RequestQueueInMemoryStore() {}

RequestQueueInMemoryStore::~RequestQueueInMemoryStore() {}

void RequestQueueInMemoryStore::GetRequests(
    const GetRequestsCallback& callback) {
  std::vector<SavePageRequest> result_requests;
  for (const auto& id_request_pair : requests_)
    result_requests.push_back(id_request_pair.second);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, true, result_requests));
}

void RequestQueueInMemoryStore::AddOrUpdateRequest(
    const SavePageRequest& request,
    const UpdateCallback& callback) {
  RequestsMap::iterator iter = requests_.find(request.request_id());
  if (iter != requests_.end())
    requests_.erase(iter);
  requests_.insert(std::make_pair(request.request_id(), request));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, UpdateStatus::UPDATED));
}

void RequestQueueInMemoryStore::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  // In case the |request_ids| is empty, the result will be true, but the count
  // of deleted pages will be empty.
  int count = 0;
  RequestsMap::iterator iter;
  for (auto request_id : request_ids) {
    iter = requests_.find(request_id);
    if (iter != requests_.end()) {
      requests_.erase(iter);
      ++count;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, true, count));
}

void RequestQueueInMemoryStore::Reset(const ResetCallback& callback) {
  requests_.clear();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

}  // namespace offline_pages
