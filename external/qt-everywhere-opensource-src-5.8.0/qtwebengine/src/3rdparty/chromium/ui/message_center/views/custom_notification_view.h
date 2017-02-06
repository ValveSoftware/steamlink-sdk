// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_CUSTOM_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_CUSTOM_NOTIFICATION_VIEW_H_

#include "base/macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

// View for notification with NOTIFICATION_TYPE_CUSTOM that hosts the custom
// content of the notification.
class MESSAGE_CENTER_EXPORT CustomNotificationView : public MessageView {
 public:
  CustomNotificationView(MessageCenterController* controller,
                         const Notification& notification);
  ~CustomNotificationView() override;

  // Overidden from MessageView:
  void SetDrawBackgroundAsActive(bool active) override;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;

 private:
  friend class CustomNotificationViewTest;

  // The view for the custom content. Owned by view hierarchy.
  views::View* contents_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CustomNotificationView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_CUSTOM_NOTIFICATION_VIEW_H_
