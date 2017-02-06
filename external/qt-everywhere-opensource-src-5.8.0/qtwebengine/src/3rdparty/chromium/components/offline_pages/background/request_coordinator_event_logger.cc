// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/offline_pages/background/request_coordinator_event_logger.h"

namespace offline_pages {

void RequestCoordinatorEventLogger::RecordSavePageRequestUpdated(
    const std::string& name_space,
    const std::string& new_status,
    int64_t id) {
  RecordActivity("Save page request for ID: " + std::to_string(id) +
                 " and namespace: " + name_space +
                 " has been updated with status " + new_status);
}

}  // namespace offline_pages
