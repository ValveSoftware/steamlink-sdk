// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/message_center/message_center_tray_delegate.h"

namespace message_center {

class MessageCenter;
class MessageCenterTray;

// A message center tray delegate which does nothing.
class FakeMessageCenterTrayDelegate : public MessageCenterTrayDelegate {
 public:
  explicit FakeMessageCenterTrayDelegate(MessageCenter* message_center);
  ~FakeMessageCenterTrayDelegate() override;

  // Overridden from MessageCenterTrayDelegate:
  void OnMessageCenterTrayChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter() override;
  void HideMessageCenter() override;
  bool ShowNotifierSettings() override;
  bool IsContextMenuEnabled() const override;
  MessageCenterTray* GetMessageCenterTray() override;

 private:
  std::unique_ptr<MessageCenterTray> tray_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageCenterTrayDelegate);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_
