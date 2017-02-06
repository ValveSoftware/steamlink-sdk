// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_picker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

RequestPicker::RequestPicker(
    RequestQueue* requestQueue)
    : queue_(requestQueue),
      weak_ptr_factory_(this) {}

RequestPicker::~RequestPicker() {}

void RequestPicker::ChooseNextRequest(
    RequestCoordinator::RequestPickedCallback picked_callback,
    RequestCoordinator::RequestQueueEmptyCallback empty_callback) {
  picked_callback_ = picked_callback;
  empty_callback_ = empty_callback;
  // Get all requests from queue (there is no filtering mechanism).
  queue_->GetRequests(base::Bind(&RequestPicker::GetRequestResultCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void RequestPicker::GetRequestResultCallback(
    RequestQueue::GetRequestsResult,
    const std::vector<SavePageRequest>& requests) {
  // If there is nothing to do, return right away.
  if (requests.size() == 0) {
    empty_callback_.Run();
    return;
  }

  // Pick the most deserving request for our conditions.
  const SavePageRequest& picked_request = requests[0];

  // When we have a best request to try next, get the request coodinator to
  // start it.
  picked_callback_.Run(picked_request);
}

}  // namespace offline_pages
