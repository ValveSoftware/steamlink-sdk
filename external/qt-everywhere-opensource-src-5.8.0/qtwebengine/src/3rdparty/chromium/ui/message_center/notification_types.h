// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_TYPES_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_TYPES_H_

#include "ui/message_center/message_center_export.h"

namespace message_center {

// Keys for optional fields in Notification.
MESSAGE_CENTER_EXPORT extern const char kPriorityKey[];
MESSAGE_CENTER_EXPORT extern const char kTimestampKey[];
MESSAGE_CENTER_EXPORT extern const char kButtonOneTitleKey[];
MESSAGE_CENTER_EXPORT extern const char kButtonOneIconUrlKey[];
MESSAGE_CENTER_EXPORT extern const char kButtonTwoTitleKey[];
MESSAGE_CENTER_EXPORT extern const char kButtonTwoIconUrlKey[];
MESSAGE_CENTER_EXPORT extern const char kExpandedMessageKey[];
MESSAGE_CENTER_EXPORT extern const char kImageUrlKey[];
MESSAGE_CENTER_EXPORT extern const char kItemsKey[];
MESSAGE_CENTER_EXPORT extern const char kItemTitleKey[];
MESSAGE_CENTER_EXPORT extern const char kItemMessageKey[];
// This key should not be used by the extension API handler. It's not allowed
// to use it there, it's used to cancel timeout for webkit notifications.
MESSAGE_CENTER_EXPORT extern const char kPrivateNeverTimeoutKey[];

// Notification types. Note that the values in this enumeration are being
// recoded in a histogram, updates should not change the entries' values.
enum NotificationType {
  NOTIFICATION_TYPE_SIMPLE = 0,
  NOTIFICATION_TYPE_BASE_FORMAT = 1,
  NOTIFICATION_TYPE_IMAGE = 2,
  NOTIFICATION_TYPE_MULTIPLE = 3,
  NOTIFICATION_TYPE_PROGRESS = 4,  // Notification with progress bar.
  NOTIFICATION_TYPE_CUSTOM = 5,

  // Add new values before this line.
  NOTIFICATION_TYPE_LAST = NOTIFICATION_TYPE_PROGRESS
};

enum NotificationPriority {
  MIN_PRIORITY = -2,
  LOW_PRIORITY = -1,
  DEFAULT_PRIORITY = 0,
  HIGH_PRIORITY = 1,
  MAX_PRIORITY = 2,

  // Top priority for system-level notifications.. This can't be set from
  // kPriorityKey, instead you have to call SetSystemPriority() of
  // Notification object.
  SYSTEM_PRIORITY = 3,
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_TYPES_H_
