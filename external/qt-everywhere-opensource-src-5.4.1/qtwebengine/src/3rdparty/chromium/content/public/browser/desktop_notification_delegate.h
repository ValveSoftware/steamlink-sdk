// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_NOTIFICATION_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_NOTIFICATION_DELEGATE_H_

namespace content {

// A delegate used by ContentBrowserClient::ShowDesktopNotification to report
// the result of a desktop notification.
class DesktopNotificationDelegate {
 public:
  virtual ~DesktopNotificationDelegate() {}

  // The notification was shown.
  virtual void NotificationDisplayed() = 0;

  // The notification couldn't be shown due to an error.
  virtual void NotificationError() = 0;

  // The notification was closed.
  virtual void NotificationClosed(bool by_user) = 0;

  // The user clicked on the notification.
  virtual void NotificationClick() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_NOTIFICATION_DELEGATE_H_
