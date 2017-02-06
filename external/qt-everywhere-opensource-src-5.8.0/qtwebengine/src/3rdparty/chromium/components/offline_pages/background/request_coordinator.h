// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/request_coordinator_event_logger.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/scheduler.h"
#include "url/gurl.h"

namespace offline_pages {

struct ClientId;
class OfflinerPolicy;
class OfflinerFactory;
class Offliner;
class RequestPicker;
class SavePageRequest;
class Scheduler;

// Coordinates queueing and processing save page later requests.
class RequestCoordinator : public KeyedService {
 public:
  // Callback to report when the processing of a triggered task is complete.
  typedef base::Callback<void(const SavePageRequest& request)>
      RequestPickedCallback;
  typedef base::Callback<void()> RequestQueueEmptyCallback;

  RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                     std::unique_ptr<OfflinerFactory> factory,
                     std::unique_ptr<RequestQueue> queue,
                     std::unique_ptr<Scheduler> scheduler);

  ~RequestCoordinator() override;

  // Queues |request| to later load and save when system conditions allow.
  // Returns true if the page could be queued successfully.
  bool SavePageLater(const GURL& url, const ClientId& client_id);

  // Starts processing of one or more queued save page later requests.
  // Returns whether processing was started and that caller should expect
  // a callback. If processing was already active, returns false.
  bool StartProcessing(const DeviceConditions& device_conditions,
                       const base::Callback<void(bool)>& callback);

  // Stops the current request processing if active. This is a way for
  // caller to abort processing; otherwise, processing will complete on
  // its own. In either case, the callback will be called when processing
  // is stopped or complete.
  void StopProcessing();

  // TODO(dougarnett): Move to OfflinerPolicy in some form.
  const Scheduler::TriggerConditions& GetTriggerConditionsForUserRequest();

  // A way for tests to set the callback in use when an operation is over.
  void SetProcessingCallbackForTest(const base::Callback<void(bool)> callback) {
    scheduler_callback_ = callback;
  }

  // Returns the request queue used for requests.  Coordinator keeps ownership.
  RequestQueue* queue() { return queue_.get(); }

  // Return an unowned pointer to the Scheduler.
  Scheduler* scheduler() { return scheduler_.get(); }

  // Returns the status of the most recent offlining.
  Offliner::RequestStatus last_offlining_status() {
    return last_offlining_status_;
  }

  bool is_busy() {
    return is_busy_;
  }

  // Tracks whether the last offlining attempt got canceled.  This is reset by
  // the next StartProcessing() call.
  bool is_canceled() {
    return is_canceled_;
  }

  OfflineEventLogger* GetLogger() {
    return &event_logger_;
  }

 private:
  void AddRequestResultCallback(RequestQueue::AddRequestResult result,
                                const SavePageRequest& request);

  void UpdateRequestCallback(RequestQueue::UpdateRequestResult result);

  // Callback from the request picker when it has chosen our next request.
  void RequestPicked(const SavePageRequest& request);

  // Callback from the request picker when no more requests are in the queue.
  void RequestQueueEmpty();

  void SendRequestToOffliner(const SavePageRequest& request);

  // Called by the offliner when an offlining request is completed. (and by
  // tests).
  void OfflinerDoneCallback(const SavePageRequest& request,
                            Offliner::RequestStatus status);

  void TryNextRequest();

  // Returns the appropriate offliner to use, getting a new one from the factory
  // if needed.
  void GetOffliner();

  friend class RequestCoordinatorTest;

  // The offliner can only handle one request at a time - if the offliner is
  // busy, prevent other requests.  This flag marks whether the offliner is in
  // use.
  bool is_busy_;
  // True if the current request has been canceled.
  bool is_canceled_;
  // Unowned pointer to the current offliner, if any.
  Offliner* offliner_;
  // RequestCoordinator takes over ownership of the policy
  std::unique_ptr<OfflinerPolicy> policy_;
  // OfflinerFactory.  Used to create offline pages. Owned.
  std::unique_ptr<OfflinerFactory> factory_;
  // RequestQueue.  Used to store incoming requests. Owned.
  std::unique_ptr<RequestQueue> queue_;
  // Scheduler. Used to request a callback when network is available.  Owned.
  std::unique_ptr<Scheduler> scheduler_;
  // Status of the most recent offlining.
  Offliner::RequestStatus last_offlining_status_;
  // Class to choose which request to schedule next
  std::unique_ptr<RequestPicker> picker_;
  // Calling this returns to the scheduler across the JNI bridge.
  base::Callback<void(bool)> scheduler_callback_;
  // Logger to record events.
  RequestCoordinatorEventLogger event_logger_;
  // Allows us to pass a weak pointer to callbacks.
  base::WeakPtrFactory<RequestCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RequestCoordinator);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_H_
