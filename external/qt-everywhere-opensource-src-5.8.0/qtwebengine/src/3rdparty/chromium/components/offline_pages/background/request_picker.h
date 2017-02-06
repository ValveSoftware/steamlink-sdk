// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/request_queue.h"

namespace offline_pages {
class RequestPicker {
 public:
  RequestPicker(RequestQueue* requestQueue);

  ~RequestPicker();

  // Choose which request we should process next based on the current
  // conditions, and call back to the RequestCoordinator when we have one.
  void ChooseNextRequest(
      RequestCoordinator::RequestPickedCallback picked_callback,
      RequestCoordinator::RequestQueueEmptyCallback empty_callback);

 private:
  // Callback for the GetRequest results to be delivered.
  void GetRequestResultCallback(RequestQueue::GetRequestsResult result,
                                const std::vector<SavePageRequest>& results);

  // unowned pointer to the request queue.
  RequestQueue* queue_;
  // Callback for when we are done picking a request to do next.
  RequestCoordinator::RequestPickedCallback picked_callback_;
  // Callback for when there are no more reqeusts to pick.
  RequestCoordinator::RequestQueueEmptyCallback empty_callback_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestPicker> weak_ptr_factory_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_PICKER_H_
