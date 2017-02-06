// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_picker.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

namespace {
// TODO(dougarnett/petewil): Move to Policy object. Also consider lower minimum
// battery percentage once there is some processing time limits in place.
const Scheduler::TriggerConditions kUserRequestTriggerConditions(
    false /* require_power_connected */,
    50 /* minimum_battery_percentage */,
    false /* require_unmetered_network */);
}  // namespace

RequestCoordinator::RequestCoordinator(std::unique_ptr<OfflinerPolicy> policy,
                                       std::unique_ptr<OfflinerFactory> factory,
                                       std::unique_ptr<RequestQueue> queue,
                                       std::unique_ptr<Scheduler> scheduler)
    : is_busy_(false),
      is_canceled_(false),
      offliner_(nullptr),
      policy_(std::move(policy)),
      factory_(std::move(factory)),
      queue_(std::move(queue)),
      scheduler_(std::move(scheduler)),
      last_offlining_status_(Offliner::RequestStatus::UNKNOWN),
      weak_ptr_factory_(this) {
  DCHECK(policy_ != nullptr);
  picker_.reset(new RequestPicker(queue_.get()));
}

RequestCoordinator::~RequestCoordinator() {}

bool RequestCoordinator::SavePageLater(
    const GURL& url, const ClientId& client_id) {
  DVLOG(2) << "URL is " << url << " " << __FUNCTION__;

  // TODO(petewil): We need a robust scheme for allocating new IDs.
  static int64_t id = 0;

  // Build a SavePageRequest.
  // TODO(petewil): Use something like base::Clock to help in testing.
  offline_pages::SavePageRequest request(
      id++, url, client_id, base::Time::Now());

  // Put the request on the request queue.
  queue_->AddRequest(request,
                     base::Bind(&RequestCoordinator::AddRequestResultCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void RequestCoordinator::AddRequestResultCallback(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {

  // Inform the scheduler that we have an outstanding task.
  // TODO(petewil): Determine trigger conditions from policy.
  scheduler_->Schedule(GetTriggerConditionsForUserRequest());
}

// Called in response to updating a request in the request queue.
void RequestCoordinator::UpdateRequestCallback(
    RequestQueue::UpdateRequestResult result) {}

void RequestCoordinator::StopProcessing() {
  is_canceled_ = true;
  if (offliner_ && is_busy_)
    offliner_->Cancel();
}

// Returns true if the caller should expect a callback, false otherwise. For
// instance, this would return false if a request is already in progress.
bool RequestCoordinator::StartProcessing(
    const DeviceConditions& device_conditions,
    const base::Callback<void(bool)>& callback) {
  if (is_busy_) return false;

  is_canceled_ = false;
  scheduler_callback_ = callback;
  // TODO(petewil): Check existing conditions (should be passed down from
  // BackgroundTask)

  TryNextRequest();

  return true;
}

void RequestCoordinator::TryNextRequest() {
  // Choose a request to process that meets the available conditions.
  // This is an async call, and returns right away.
  picker_->ChooseNextRequest(
      base::Bind(&RequestCoordinator::RequestPicked,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&RequestCoordinator::RequestQueueEmpty,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Called by the request picker when a request has been picked.
void RequestCoordinator::RequestPicked(const SavePageRequest& request) {
  // Send the request on to the offliner.
  SendRequestToOffliner(request);
}

void RequestCoordinator::RequestQueueEmpty() {
  // Clear the outstanding "safety" task in the scheduler.
  scheduler_->Unschedule();
  // Return control to the scheduler when there is no more to do.
  scheduler_callback_.Run(true);
}

void RequestCoordinator::SendRequestToOffliner(const SavePageRequest& request) {
  // Check that offlining didn't get cancelled while performing some async
  // steps.
  if (is_canceled_)
    return;

  GetOffliner();
  if (!offliner_) {
    DVLOG(0) << "Unable to create Offliner. "
             << "Cannot background offline page.";
    return;
  }

  DCHECK(!is_busy_);
  is_busy_ = true;

  // Start the load and save process in the offliner (Async).
  offliner_->LoadAndSave(request,
                         base::Bind(&RequestCoordinator::OfflinerDoneCallback,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void RequestCoordinator::OfflinerDoneCallback(const SavePageRequest& request,
                                              Offliner::RequestStatus status) {
  DVLOG(2) << "offliner finished, saved: "
           << (status == Offliner::RequestStatus::SAVED) << ", status: "
           << (int) status << ", " << __FUNCTION__;
  event_logger_.RecordSavePageRequestUpdated(
      request.client_id().name_space,
      "Saved",
      request.request_id());
  last_offlining_status_ = status;

  is_busy_ = false;

  // If the request succeeded, remove it from the Queue and maybe schedule
  // another one.
  if (status == Offliner::RequestStatus::SAVED) {
    queue_->RemoveRequest(request.request_id(),
                          base::Bind(&RequestCoordinator::UpdateRequestCallback,
                                     weak_ptr_factory_.GetWeakPtr()));

    // TODO(petewil): Check time budget. Return to the scheduler if we are out.

    // Start another request if we have time.
    TryNextRequest();
  }
}

const Scheduler::TriggerConditions&
RequestCoordinator::GetTriggerConditionsForUserRequest() {
  return kUserRequestTriggerConditions;
}

void RequestCoordinator::GetOffliner() {
  if (!offliner_) {
    offliner_ = factory_->GetOffliner(policy_.get());
  }
}

}  // namespace offline_pages
