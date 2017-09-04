// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_USER_ID_TRACKER_OBSERVER_H_
#define SERVICES_UI_WS_USER_ID_TRACKER_OBSERVER_H_

#include <stdint.h>

#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class UserIdTrackerObserver {
 public:
  virtual void OnActiveUserIdChanged(const UserId& previously_active_id,
                                     const UserId& active_id) {}
  virtual void OnUserIdAdded(const UserId& id) {}
  virtual void OnUserIdRemoved(const UserId& id) {}

 protected:
  virtual ~UserIdTrackerObserver() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_USER_ID_TRACKER_OBSERVER_H_
