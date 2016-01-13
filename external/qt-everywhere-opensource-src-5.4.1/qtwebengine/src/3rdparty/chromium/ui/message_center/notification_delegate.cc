// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

namespace message_center {

// NotificationDelegate:

bool NotificationDelegate::HasClickedListener() { return false; }

void NotificationDelegate::ButtonClick(int button_index) {}

// HandleNotificationClickedDelegate:

HandleNotificationClickedDelegate::HandleNotificationClickedDelegate(
    const base::Closure& closure)
    : closure_(closure) {
}

HandleNotificationClickedDelegate::~HandleNotificationClickedDelegate() {
}

void HandleNotificationClickedDelegate::Display() {
}

void HandleNotificationClickedDelegate::Error() {
}

void HandleNotificationClickedDelegate::Close(bool by_user) {
}

bool HandleNotificationClickedDelegate::HasClickedListener() {
  return !closure_.is_null();
}

void HandleNotificationClickedDelegate::Click() {
  if (!closure_.is_null())
    closure_.Run();
}

void HandleNotificationClickedDelegate::ButtonClick(int button_index) {
}

// HandleNotificationButtonClickDelegate:

HandleNotificationButtonClickDelegate::HandleNotificationButtonClickDelegate(
    const ButtonClickCallback& button_callback)
    : button_callback_(button_callback) {
}

HandleNotificationButtonClickDelegate::
    ~HandleNotificationButtonClickDelegate() {
}

void HandleNotificationButtonClickDelegate::Display() {
}

void HandleNotificationButtonClickDelegate::Error() {
}

void HandleNotificationButtonClickDelegate::Close(bool by_user) {
}

void HandleNotificationButtonClickDelegate::Click() {
}

void HandleNotificationButtonClickDelegate::ButtonClick(int button_index) {
  if (!button_callback_.is_null())
    button_callback_.Run(button_index);
}

}  // namespace message_center
