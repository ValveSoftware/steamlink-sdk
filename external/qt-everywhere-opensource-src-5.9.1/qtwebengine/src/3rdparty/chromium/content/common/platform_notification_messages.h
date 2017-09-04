// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages for platform-native notifications using the Web Notification API.
// Multiply-included message file, hence no include guard.

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "content/public/common/common_param_traits_macros.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "ipc/ipc_message_macros.h"

// Singly-included section for type definitions.
#ifndef CONTENT_COMMON_PLATFORM_NOTIFICATION_MESSAGES_H_
#define CONTENT_COMMON_PLATFORM_NOTIFICATION_MESSAGES_H_

// Defines the pair of [notification id] => [notification data] used when
// getting the notifications for a given Service Worker registration.
using PersistentNotificationInfo =
    std::pair<std::string, content::PlatformNotificationData>;

#endif  // CONTENT_COMMON_PLATFORM_NOTIFICATION_MESSAGES_H_

#define IPC_MESSAGE_START PlatformNotificationMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(
    content::PlatformNotificationData::Direction,
    content::PlatformNotificationData::DIRECTION_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(content::PlatformNotificationActionType,
                          content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT)

IPC_STRUCT_TRAITS_BEGIN(content::PlatformNotificationAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(icon)
  IPC_STRUCT_TRAITS_MEMBER(placeholder)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::PlatformNotificationData)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(direction)
  IPC_STRUCT_TRAITS_MEMBER(lang)
  IPC_STRUCT_TRAITS_MEMBER(body)
  IPC_STRUCT_TRAITS_MEMBER(tag)
  IPC_STRUCT_TRAITS_MEMBER(image)
  IPC_STRUCT_TRAITS_MEMBER(icon)
  IPC_STRUCT_TRAITS_MEMBER(badge)
  IPC_STRUCT_TRAITS_MEMBER(vibration_pattern)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(renotify)
  IPC_STRUCT_TRAITS_MEMBER(silent)
  IPC_STRUCT_TRAITS_MEMBER(require_interaction)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(actions)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::NotificationResources)
  IPC_STRUCT_TRAITS_MEMBER(image)
  IPC_STRUCT_TRAITS_MEMBER(notification_icon)
  IPC_STRUCT_TRAITS_MEMBER(badge)
  IPC_STRUCT_TRAITS_MEMBER(action_icons)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Informs the renderer that the browser has displayed the notification.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidShow,
                     int /* notification_id */)

// Informs the renderer that the notification has been closed.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidClose,
                     int /* notification_id */)

// Informs the renderer that the notification has been clicked on.
IPC_MESSAGE_CONTROL1(PlatformNotificationMsg_DidClick,
                     int /* notification_id */)

// Reply to PlatformNotificationHostMsg_ShowPersistent indicating that a
// persistent notification has been shown on the platform (if |success| is
// true), or that an unspecified error occurred.
IPC_MESSAGE_CONTROL2(PlatformNotificationMsg_DidShowPersistent,
                     int /* request_id */,
                     bool /* success */)

// Reply to PlatformNotificationHostMsg_GetNotifications sharing a vector of
// available notifications per the request's constraints.
IPC_MESSAGE_CONTROL2(PlatformNotificationMsg_DidGetNotifications,
                     int /* request_id */,
                     std::vector<PersistentNotificationInfo>
                         /* notifications */)

// Messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL4(
    PlatformNotificationHostMsg_Show,
    int /* non_persistent_notification_id */,
    GURL /* origin */,
    content::PlatformNotificationData /* notification_data */,
    content::NotificationResources /* notification_resources */)

IPC_MESSAGE_CONTROL5(
    PlatformNotificationHostMsg_ShowPersistent,
    int /* request_id */,
    int64_t /* service_worker_registration_id */,
    GURL /* origin */,
    content::PlatformNotificationData /* notification_data */,
    content::NotificationResources /* notification_resources */)

IPC_MESSAGE_CONTROL4(PlatformNotificationHostMsg_GetNotifications,
                     int /* request_id */,
                     int64_t /* service_worker_registration_id */,
                     GURL /* origin */,
                     std::string /* filter_tag */)

IPC_MESSAGE_CONTROL3(PlatformNotificationHostMsg_Close,
                     GURL /* origin */,
                     std::string /* tag */,
                     int /* non_persistent_notification_id */)

IPC_MESSAGE_CONTROL3(PlatformNotificationHostMsg_ClosePersistent,
                     GURL /* origin */,
                     std::string /* tag */,
                     std::string /* notification_id */)
