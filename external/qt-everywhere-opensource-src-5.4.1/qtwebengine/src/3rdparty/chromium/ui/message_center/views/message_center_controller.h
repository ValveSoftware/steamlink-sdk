// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/models/menu_model.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {

// Interface used by views to report clicks and other user actions. The views
// by themselves do not know how to perform those operations, they ask
// MessageCenterController to do them. Implemented by MessageCeneterView and
// MessagePopupCollection.
class MessageCenterController {
 public:
  virtual void ClickOnNotification(const std::string& notification_id) = 0;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) = 0;
  virtual scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) = 0;
  virtual bool HasClickedListener(const std::string& notification_id) = 0;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_CONTROLLER_H_
