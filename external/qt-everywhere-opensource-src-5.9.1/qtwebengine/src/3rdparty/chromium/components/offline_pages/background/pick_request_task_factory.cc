// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/pick_request_task_factory.h"

#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator_event_logger.h"
#include "components/offline_pages/background/request_notifier.h"

namespace offline_pages {

// Capture the common parameters that we will need to make a pick task,
// and use them when making tasks.  Create this once each session, and
// use it to build a picker task when needed.
PickRequestTaskFactory::PickRequestTaskFactory(
    OfflinerPolicy* policy,
    RequestNotifier* notifier,
    RequestCoordinatorEventLogger* event_logger)
    : policy_(policy), notifier_(notifier), event_logger_(event_logger) {}

PickRequestTaskFactory::~PickRequestTaskFactory() {}

std::unique_ptr<PickRequestTask> PickRequestTaskFactory::CreatePickerTask(
    RequestQueueStore* store,
    const PickRequestTask::RequestPickedCallback& picked_callback,
    const PickRequestTask::RequestNotPickedCallback& not_picked_callback,
    const PickRequestTask::RequestCountCallback& request_count_callback,
    DeviceConditions& conditions,
    std::set<int64_t>& disabled_requests) {
  std::unique_ptr<PickRequestTask> task(new PickRequestTask(
      store, policy_, notifier_, event_logger_, picked_callback,
      not_picked_callback, request_count_callback, conditions,
      disabled_requests));
  return task;
}

}  // namespace offline_pages
