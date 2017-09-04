// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/pick_request_task.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator_event_logger.h"
#include "components/offline_pages/background/request_notifier.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/save_page_request.h"

namespace {
template <typename T>
int signum(T t) {
  return (T(0) < t) - (t < T(0));
}

#define CALL_MEMBER_FUNCTION(object, ptrToMember) ((object)->*(ptrToMember))
}  // namespace

namespace offline_pages {

PickRequestTask::PickRequestTask(RequestQueueStore* store,
                                 OfflinerPolicy* policy,
                                 RequestNotifier* notifier,
                                 RequestCoordinatorEventLogger* event_logger,
                                 RequestPickedCallback picked_callback,
                                 RequestNotPickedCallback not_picked_callback,
                                 RequestCountCallback request_count_callback,
                                 DeviceConditions& device_conditions,
                                 const std::set<int64_t>& disabled_requests)
    : store_(store),
      policy_(policy),
      notifier_(notifier),
      event_logger_(event_logger),
      picked_callback_(picked_callback),
      not_picked_callback_(not_picked_callback),
      request_count_callback_(request_count_callback),
      disabled_requests_(disabled_requests),
      weak_ptr_factory_(this) {
  device_conditions_.reset(new DeviceConditions(device_conditions));
}

PickRequestTask::~PickRequestTask() {}

void PickRequestTask::Run() {
  // Get all the requests from the queue, we will classify them in the callback.
  store_->GetRequests(base::Bind(&PickRequestTask::ChooseAndPrune,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void PickRequestTask::ChooseAndPrune(
    bool success,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  // If there is nothing to do, return right away.
  if (requests.empty()) {
    request_count_callback_.Run(requests.size(), 0);
    not_picked_callback_.Run(false);
    TaskComplete();
    return;
  }

  // Get the expired requests to be removed from the queue, and the valid ones
  // from which to pick the next request.
  std::vector<std::unique_ptr<SavePageRequest>> valid_requests;
  std::vector<int64_t> expired_request_ids;
  SplitRequests(std::move(requests), &valid_requests, &expired_request_ids);

  // Continue processing by choosing a request.
  ChooseRequestAndCallback(std::move(valid_requests));

  // Continue processing by handling expired requests, if any.
  if (expired_request_ids.size() == 0) {
    TaskComplete();
    return;
  }

  RemoveStaleRequests(std::move(expired_request_ids));
}

void PickRequestTask::ChooseRequestAndCallback(
    std::vector<std::unique_ptr<SavePageRequest>> valid_requests) {
  // Pick the most deserving request for our conditions.
  const SavePageRequest* picked_request = nullptr;

  RequestCompareFunction comparator = nullptr;

  // Choose which comparison function to use based on policy.
  if (policy_->RetryCountIsMoreImportantThanRecency())
    comparator = &PickRequestTask::RetryCountFirstCompareFunction;
  else
    comparator = &PickRequestTask::RecencyFirstCompareFunction;

  // TODO(petewil): Consider replacing this bool with a better named enum.
  bool non_user_requested_tasks_remaining = false;

  size_t available_request_count = 0;

  // Iterate once through the requests, keeping track of best candidate.
  for (unsigned i = 0; i < valid_requests.size(); ++i) {
    // If the  request is on the disabled list, skip it.
    auto search = disabled_requests_.find(valid_requests[i]->request_id());
    if (search != disabled_requests_.end())
      continue;
    // If there are non-user-requested tasks remaining, we need to make sure
    // that they are scheduled after we run out of user requested tasks. Here we
    // detect if any exist. If we don't find any user-requested tasks, we will
    // inform the "not_picked_callback_" that it needs to schedule a task for
    // non-user-requested items, which have different network and power needs.
    if (!valid_requests[i]->user_requested())
      non_user_requested_tasks_remaining = true;
    if (valid_requests[i]->request_state() ==
        SavePageRequest::RequestState::AVAILABLE) {
      available_request_count++;
    }
    if (!RequestConditionsSatisfied(valid_requests[i].get()))
      continue;
    if (IsNewRequestBetter(picked_request, valid_requests[i].get(), comparator))
      picked_request = valid_requests[i].get();
  }

  // Report the request queue counts.
  request_count_callback_.Run(valid_requests.size(), available_request_count);

  // If we have a best request to try next, get the request coodinator to
  // start it.  Otherwise return that we have no candidates.
  if (picked_request != nullptr)
    picked_callback_.Run(*picked_request);
  else
    not_picked_callback_.Run(non_user_requested_tasks_remaining);
}

// Continue the async part of the processing by deleting the expired requests.
// TODO(petewil): Does that really need to be done on the task queue? Hard to
// see how we need to wait for it before starting the next task. OTOH, we'd hate
// to do a second slow DB operation to get entries a second time, and waiting
// until this is done will make sure other gets don't see these old entries.
// Consider moving this to a fresh task type to clean the queue.
void PickRequestTask::RemoveStaleRequests(
    std::vector<int64_t> stale_request_ids) {
  store_->RemoveRequests(stale_request_ids,
                         base::Bind(&PickRequestTask::OnRequestsExpired,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void PickRequestTask::OnRequestsExpired(
    std::unique_ptr<UpdateRequestsResult> result) {
  RequestNotifier::BackgroundSavePageResult save_page_result(
      RequestNotifier::BackgroundSavePageResult::EXPIRED);
  for (const auto& request : result->updated_items) {
    event_logger_->RecordDroppedSavePageRequest(
        request.client_id().name_space, save_page_result, request.request_id());
    notifier_->NotifyCompleted(request, save_page_result);
  }

  // The task is now done, return control to the task queue.
  TaskComplete();
}

void PickRequestTask::SplitRequests(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    std::vector<std::unique_ptr<SavePageRequest>>* valid_requests,
    std::vector<int64_t>* expired_request_ids) {
  for (auto& request : requests) {
    if (base::Time::Now() - request->creation_time() >=
        base::TimeDelta::FromSeconds(kRequestExpirationTimeInSeconds)) {
      expired_request_ids->push_back(request->request_id());
    } else {
      valid_requests->push_back(std::move(request));
    }
  }
}

// Filter out requests that don't meet the current conditions.  For instance, if
// this is a predictive request, and we are not on WiFi, it should be ignored
// this round.
bool PickRequestTask::RequestConditionsSatisfied(
    const SavePageRequest* request) {
  // If the user did not request the page directly, make sure we are connected
  // to power and have WiFi and sufficient battery remaining before we take this
  // request.
  if (!device_conditions_->IsPowerConnected() &&
      policy_->PowerRequired(request->user_requested())) {
    return false;
  }

  if (device_conditions_->GetNetConnectionType() !=
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI &&
      policy_->UnmeteredNetworkRequired(request->user_requested())) {
    return false;
  }

  if (device_conditions_->GetBatteryPercentage() <
      policy_->BatteryPercentageRequired(request->user_requested())) {
    return false;
  }

  // If we have already started this page the max number of times, it is not
  // eligible to try again.
  if (request->started_attempt_count() >= policy_->GetMaxStartedTries())
    return false;

  // If we have already completed trying this page the max number of times, it
  // is not eligible to try again.
  if (request->completed_attempt_count() >= policy_->GetMaxCompletedTries())
    return false;

  // If the request is paused, do not consider it.
  if (request->request_state() == SavePageRequest::RequestState::PAUSED)
    return false;

  // If the request is expired, do not consider it.
  base::TimeDelta requestAge = base::Time::Now() - request->creation_time();
  if (requestAge > base::TimeDelta::FromSeconds(
                       policy_->GetRequestExpirationTimeInSeconds()))
    return false;

  // If this request is not active yet, return false.
  // TODO(petewil): If the only reason we return nothing to do is that we have
  // inactive requests, we still want to try again later after their activation
  // time elapses, we shouldn't take ourselves completely off the scheduler.
  if (request->activation_time() > base::Time::Now())
    return false;

  return true;
}

// Look at policies to decide which requests to prefer.
bool PickRequestTask::IsNewRequestBetter(const SavePageRequest* oldRequest,
                                         const SavePageRequest* newRequest,
                                         RequestCompareFunction comparator) {
  // If there is no old request, the new one is better.
  if (oldRequest == nullptr)
    return true;

  // User requested pages get priority.
  if (newRequest->user_requested() && !oldRequest->user_requested())
    return true;

  // Otherwise, use the comparison function for the current policy, which
  // returns true if the older request is better.
  return !(CALL_MEMBER_FUNCTION(this, comparator)(oldRequest, newRequest));
}

// Compare the results, checking request count before recency.  Returns true if
// left hand side is better, false otherwise.
bool PickRequestTask::RetryCountFirstCompareFunction(
    const SavePageRequest* left,
    const SavePageRequest* right) {
  // Check the attempt count.
  int result = CompareRetryCount(left, right);

  if (result != 0)
    return (result > 0);

  // If we get here, the attempt counts were the same, so check recency.
  result = CompareCreationTime(left, right);

  return (result > 0);
}

// Compare the results, checking recency before request count. Returns true if
// left hand side is better, false otherwise.
bool PickRequestTask::RecencyFirstCompareFunction(
    const SavePageRequest* left,
    const SavePageRequest* right) {
  // Check the recency.
  int result = CompareCreationTime(left, right);

  if (result != 0)
    return (result > 0);

  // If we get here, the recency was the same, so check the attempt count.
  result = CompareRetryCount(left, right);

  return (result > 0);
}

// Compare left and right side, returning 1 if the left side is better
// (preferred by policy), 0 if the same, and -1 if the right side is better.
int PickRequestTask::CompareRetryCount(const SavePageRequest* left,
                                       const SavePageRequest* right) {
  // Check the attempt count.
  int result = signum(left->completed_attempt_count() -
                      right->completed_attempt_count());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (policy_->ShouldPreferUntriedRequests())
    result *= -1;

  return result;
}

// Compare left and right side, returning 1 if the left side is better
// (preferred by policy), 0 if the same, and -1 if the right side is better.
int PickRequestTask::CompareCreationTime(const SavePageRequest* left,
                                         const SavePageRequest* right) {
  // Check the recency.
  base::TimeDelta difference = left->creation_time() - right->creation_time();
  int result = signum(difference.InMilliseconds());

  // Flip the direction of comparison if policy prefers fewer retries.
  if (policy_->ShouldPreferEarlierRequests())
    result *= -1;

  return result;
}

}  // namespace offline_pages
