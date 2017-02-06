// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {

// Use this object in conjunction with the
// |WebContentsObserver::AccessibilityEventReceived| method.
struct CONTENT_EXPORT AXEventNotificationDetails {
 public:
  AXEventNotificationDetails();
  AXEventNotificationDetails(const AXEventNotificationDetails& other);
  ~AXEventNotificationDetails();

  ui::AXTreeUpdate update;
  ui::AXEvent event_type;
  int id;
  int ax_tree_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_
