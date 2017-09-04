// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_

#include <string>

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {
class NotificationBlocker;

// An observer class for the change of notifications in the MessageCenter.
class MESSAGE_CENTER_EXPORT MessageCenterObserver {
 public:
  virtual ~MessageCenterObserver() {}

  // Called when the notification associated with |notification_id| is added
  // to the notification_list.
  virtual void OnNotificationAdded(const std::string& notification_id) {}

  // Called when the notification associated with |notification_id| is removed
  // from the notification_list.
  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) {}

  // Called when the contents of the notification associated with
  // |notification_id| is updated.
  virtual void OnNotificationUpdated(const std::string& notification_id) {}

  // Called when a click event happens on the notification associated with
  // |notification_id|.
  virtual void OnNotificationClicked(const std::string& notification_id) {}

  // Called when a click event happens on a button indexed by |button_index|
  // of the notification associated with |notification_id|.
  virtual void OnNotificationButtonClicked(const std::string& notification_id,
                                           int button_index) {}

  // Called when notification settings button is clicked.
  virtual void OnNotificationSettingsClicked() {}

  // Called when the notification associated with |notification_id| is actually
  // displayed.
  virtual void OnNotificationDisplayed(
      const std::string& notification_id,
      const DisplaySource source) {}

  // Called when the notification center is shown or hidden.
  virtual void OnCenterVisibilityChanged(Visibility visibility) {}

  // Called whenever the quiet mode changes as a result of user action or when
  // quiet mode expires.
  virtual void OnQuietModeChanged(bool in_quiet_mode) {}

  // Called when the user locks (or unlocks) the screen.
  virtual void OnLockedStateChanged(bool locked) {}

  // Called when the blocking state of |blocker| is changed.
  virtual void OnBlockingStateChanged(NotificationBlocker* blocker) {}
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_OBSERVER_H_
