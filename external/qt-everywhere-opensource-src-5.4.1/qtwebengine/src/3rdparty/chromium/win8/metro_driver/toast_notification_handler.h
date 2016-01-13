// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_

#include <windows.ui.notifications.h>

#include "base/strings/string16.h"
#include "base/win/metro.h"

// Provides functionality to display a metro style toast notification.
class ToastNotificationHandler {
 public:
  // Holds information about a desktop notification to be displayed.
  struct DesktopNotification {
    std::string origin_url;
    std::string icon_url;
    base::string16 title;
    base::string16 body;
    base::string16 display_source;
    std::string id;
    base::win::MetroNotificationClickedHandler notification_handler;
    base::string16 notification_context;

    DesktopNotification(const char* notification_origin,
                        const char* notification_icon,
                        const wchar_t* notification_title,
                        const wchar_t* notification_body,
                        const wchar_t* notification_display_source,
                        const char* notification_id,
                        base::win::MetroNotificationClickedHandler handler,
                        const wchar_t* handler_context);

    DesktopNotification();
  };

  ToastNotificationHandler();
  ~ToastNotificationHandler();

  void DisplayNotification(const DesktopNotification& notification);
  void CancelNotification();

  HRESULT OnActivate(winui::Notifications::IToastNotification* notification,
                     IInspectable* inspectable);

 private:
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier_;
  mswr::ComPtr<winui::Notifications::IToastNotification> notification_;
  EventRegistrationToken activated_token_;
  DesktopNotification notification_info_;
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_TOAST_NOTIFICATION_HANDLER_H_
