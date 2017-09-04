// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PAGE_NOTIFICATION_DELEGATE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PAGE_NOTIFICATION_DELEGATE_H_

#include <string>

#include "content/public/browser/desktop_notification_delegate.h"

namespace content {

// A delegate used by the notification service to report the results of platform
// notification interactions to Web Notifications whose lifetime is tied to
// that of the page displaying them.
class PageNotificationDelegate : public DesktopNotificationDelegate {
 public:
  PageNotificationDelegate(int render_process_id,
                           int non_persistent_notification_id,
                           const std::string& notification_id);
  ~PageNotificationDelegate() override;

  // DesktopNotificationDelegate implementation.
  void NotificationDisplayed() override;
  void NotificationClosed() override;
  void NotificationClick() override;

 private:
  int render_process_id_;
  int non_persistent_notification_id_;
  std::string notification_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PAGE_NOTIFICATION_DELEGATE_H_
