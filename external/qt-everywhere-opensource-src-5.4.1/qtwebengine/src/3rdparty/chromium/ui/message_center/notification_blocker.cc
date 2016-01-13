// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_blocker.h"

#include "ui/message_center/message_center.h"

namespace message_center {

NotificationBlocker::NotificationBlocker(MessageCenter* message_center)
    : message_center_(message_center) {
  if (message_center_)
    message_center_->AddNotificationBlocker(this);
}

NotificationBlocker::~NotificationBlocker() {
  if (message_center_)
    message_center_->RemoveNotificationBlocker(this);
}

void NotificationBlocker::AddObserver(NotificationBlocker::Observer* observer) {
  observers_.AddObserver(observer);
}

void NotificationBlocker::RemoveObserver(
    NotificationBlocker::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NotificationBlocker::ShouldShowNotification(
    const NotifierId& notifier_id) const {
  return true;
}

void NotificationBlocker::NotifyBlockingStateChanged() {
  FOR_EACH_OBSERVER(
      NotificationBlocker::Observer, observers_, OnBlockingStateChanged(this));
}

}  // namespace message_center
