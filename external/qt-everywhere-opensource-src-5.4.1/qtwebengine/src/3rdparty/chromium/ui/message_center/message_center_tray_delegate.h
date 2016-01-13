// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_DELEGATE_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_DELEGATE_H_

#include "ui/message_center/message_center_export.h"

namespace message_center {

class MessageCenterTray;

// A MessageCenterTrayDelegate class is responsible for managing the various UI
// surfaces that should be displayed when the MessageCenter is changed.
class MESSAGE_CENTER_EXPORT MessageCenterTrayDelegate {
 public:
  virtual ~MessageCenterTrayDelegate() {};

  // Called whenever a change to the visible UI has occurred.
  virtual void OnMessageCenterTrayChanged() = 0;

  // Display the popup bubbles for new notifications to the user.  Returns true
  // if popups were actually displayed to the user.
  virtual bool ShowPopups() = 0;

  // Remove the popup bubbles from the UI.
  virtual void HidePopups() = 0;

  // Display the message center containing all undismissed notifications to the
  // user.  Returns true if the center was actually displayed to the user.
  virtual bool ShowMessageCenter() = 0;

  // Remove the message center from the UI.
  virtual void HideMessageCenter() = 0;

  // Display the notifier settings as a bubble.
  virtual bool ShowNotifierSettings() = 0;

  // Returns true if the context menu for a notification is currently enabled.
  virtual bool IsContextMenuEnabled() const = 0;

  // Show a platform-specific UI that informs the user how to open the message
  // center.
  virtual void DisplayFirstRunBalloon() {}

  virtual MessageCenterTray* GetMessageCenterTray() = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_DELEGATE_H_
