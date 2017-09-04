// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view_factory.h"

#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/custom_notification_view.h"
#include "ui/message_center/views/notification_view.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace message_center {

// static
MessageView* MessageViewFactory::Create(MessageCenterController* controller,
                                        const Notification& notification,
                                        bool top_level) {
  MessageView* notification_view = nullptr;
  switch (notification.type()) {
    case NOTIFICATION_TYPE_BASE_FORMAT:
    case NOTIFICATION_TYPE_IMAGE:
    case NOTIFICATION_TYPE_MULTIPLE:
    case NOTIFICATION_TYPE_SIMPLE:
    case NOTIFICATION_TYPE_PROGRESS:
      // All above roads lead to the generic NotificationView.
      notification_view = new NotificationView(controller, notification);
      break;
    case NOTIFICATION_TYPE_CUSTOM:
      notification_view = new CustomNotificationView(controller, notification);
      break;
    default:
      // If the caller asks for an unrecognized kind of view (entirely possible
      // if an application is running on an older version of this code that
      // doesn't have the requested kind of notification template), we'll fall
      // back to a notification instance that will provide at least basic
      // functionality.
      LOG(WARNING) << "Unable to fulfill request for unrecognized "
                   << "notification type " << notification.type() << ". "
                   << "Falling back to simple notification type.";
      notification_view = new NotificationView(controller, notification);
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Don't create shadows for notification toasts on linux wih aura.
  if (top_level)
    return notification_view;
#endif

#if defined(OS_WIN)
  // Don't create shadows for notifications on Windows under classic theme.
  if (top_level && !ui::win::IsAeroGlassEnabled()) {
    return notification_view;
  }
#endif  // OS_WIN

  notification_view->CreateShadowBorder();
  return notification_view;
}

}  // namespace message_center
