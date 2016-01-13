// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/active_notification_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "third_party/WebKit/public/web/WebNotification.h"
#include "third_party/WebKit/public/web/WebNotificationPermissionCallback.h"

using blink::WebNotification;
using blink::WebNotificationPermissionCallback;

namespace content {

ActiveNotificationTracker::ActiveNotificationTracker() {}

ActiveNotificationTracker::~ActiveNotificationTracker() {}

bool ActiveNotificationTracker::GetId(
    const WebNotification& notification, int& id) {
  ReverseTable::iterator iter = reverse_notification_table_.find(notification);
  if (iter == reverse_notification_table_.end())
    return false;
  id = iter->second;
  return true;
}

bool ActiveNotificationTracker::GetNotification(
    int id, WebNotification* notification) {
  WebNotification* lookup = notification_table_.Lookup(id);
  if (!lookup)
    return false;

  *notification = *lookup;
  return true;
}

int ActiveNotificationTracker::RegisterNotification(
    const blink::WebNotification& proxy) {
  if (reverse_notification_table_.find(proxy)
      != reverse_notification_table_.end()) {
    return reverse_notification_table_[proxy];
  } else {
    WebNotification* notification = new WebNotification(proxy);
    int id = notification_table_.Add(notification);
    reverse_notification_table_[proxy] = id;
    return id;
  }
}

void ActiveNotificationTracker::UnregisterNotification(int id) {
  // We want to free the notification after removing it from the table.
  scoped_ptr<WebNotification> notification(notification_table_.Lookup(id));
  notification_table_.Remove(id);
  DCHECK(notification.get());
  if (notification)
    reverse_notification_table_.erase(*notification);
}

void ActiveNotificationTracker::Clear() {
  while (!reverse_notification_table_.empty()) {
    ReverseTable::iterator iter = reverse_notification_table_.begin();
    UnregisterNotification((*iter).second);
  }
}

WebNotificationPermissionCallback* ActiveNotificationTracker::GetCallback(
    int id) {
  return callback_table_.Lookup(id);
}

int ActiveNotificationTracker::RegisterPermissionRequest(
    WebNotificationPermissionCallback* callback) {
  return callback_table_.Add(callback);
}

void ActiveNotificationTracker::OnPermissionRequestComplete(int id) {
  callback_table_.Remove(id);
}

}  // namespace content
