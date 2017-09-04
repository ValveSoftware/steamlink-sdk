// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_VIEW_FACTORY_H_
#define UI_MESSAGE_CENTER_MESSAGE_VIEW_FACTORY_H_

#include "ui/message_center/message_center_export.h"

namespace message_center {

class MessageCenterController;
class MessageView;
class Notification;

// Creates appropriate MessageViews for notifications depending on the
// notification type. A notification is top level if it needs to be rendered
// outside the browser window. No custom shadows are created for top level
// notifications on Linux with Aura.
class MESSAGE_CENTER_EXPORT MessageViewFactory {
 public:
  // |controller| may be NULL, but has to be set before the view is shown.
  static MessageView* Create(MessageCenterController* controller,
                             const Notification& notification,
                             bool top_level);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_VIEW_FACTORY_H_
