// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_IN_MEMORY_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_IN_MEMORY_STORE_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "components/offline_pages/background/request_queue_store.h"

namespace offline_pages {

class SavePageRequest;

// Interface for classes storing save page requests.
class RequestQueueInMemoryStore : public RequestQueueStore {
 public:
  RequestQueueInMemoryStore();
  ~RequestQueueInMemoryStore() override;

  // RequestQueueStore implementaiton.
  void GetRequests(const GetRequestsCallback& callback) override;
  void AddOrUpdateRequest(const SavePageRequest& request,
                          const UpdateCallback& callback) override;
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      const RemoveCallback& callback) override;
  void Reset(const ResetCallback& callback) override;

 private:
  typedef std::map<int64_t, SavePageRequest> RequestsMap;
  RequestsMap requests_;

  DISALLOW_COPY_AND_ASSIGN(RequestQueueInMemoryStore);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_IN_MEMORY_STORE_H_
