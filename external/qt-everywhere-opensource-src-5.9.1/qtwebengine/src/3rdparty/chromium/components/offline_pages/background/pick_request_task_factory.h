// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_FACTORY_H_

#include <stdint.h>

#include <set>

#include "components/offline_pages/background/pick_request_task.h"

namespace offline_pages {

class DeviceContitions;
class OfflinerPolicy;
class RequestCoordinatorEventLogger;
class RequestNotifier;
class RequestQueueStore;

class PickRequestTaskFactory {
 public:
  PickRequestTaskFactory(OfflinerPolicy* policy,
                         RequestNotifier* notifier,
                         RequestCoordinatorEventLogger* event_logger);

  ~PickRequestTaskFactory();

  std::unique_ptr<PickRequestTask> CreatePickerTask(
      RequestQueueStore* store,
      const PickRequestTask::RequestPickedCallback& picked_callback,
      const PickRequestTask::RequestNotPickedCallback& not_picked_callback,
      const PickRequestTask::RequestCountCallback& request_count_callback,
      DeviceConditions& conditions,
      std::set<int64_t>& disabled_requests);

 private:
  // Unowned pointer to the Policy
  OfflinerPolicy* policy_;
  // Unowned pointer to the notifier
  RequestNotifier* notifier_;
  // Unowned pointer to the EventLogger
  RequestCoordinatorEventLogger* event_logger_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_PICK_REQUEST_TASK_FACTORY_H_
