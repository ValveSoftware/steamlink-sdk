// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_EVENT_LOGGER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_EVENT_LOGGER_H_

#include <stdint.h>

#include "components/offline_pages/offline_event_logger.h"

namespace offline_pages {

class RequestCoordinatorEventLogger : public OfflineEventLogger {
 public:
  // Records that a background task with SavePageRequest |request_id|
  // has been updated.
  void RecordSavePageRequestUpdated(const std::string& name_space,
                                    const std::string& new_status,
                                    int64_t request_id);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_COORDINATOR_EVENT_LOGGER_H_
