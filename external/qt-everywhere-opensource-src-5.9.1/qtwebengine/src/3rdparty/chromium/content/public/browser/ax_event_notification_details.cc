// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_event_notification_details.h"

namespace content {

AXEventNotificationDetails::AXEventNotificationDetails()
    : event_type(ui::AX_EVENT_NONE),
      id(-1),
      ax_tree_id(-1),
      event_from(ui::AX_EVENT_FROM_NONE) {}

AXEventNotificationDetails::AXEventNotificationDetails(
    const AXEventNotificationDetails& other) = default;

AXEventNotificationDetails::~AXEventNotificationDetails() {}

AXLocationChangeNotificationDetails::AXLocationChangeNotificationDetails()
    : id(-1),
      ax_tree_id(-1) {
}

AXLocationChangeNotificationDetails::AXLocationChangeNotificationDetails(
    const AXLocationChangeNotificationDetails& other) = default;

AXLocationChangeNotificationDetails::~AXLocationChangeNotificationDetails() {}


}  // namespace content
