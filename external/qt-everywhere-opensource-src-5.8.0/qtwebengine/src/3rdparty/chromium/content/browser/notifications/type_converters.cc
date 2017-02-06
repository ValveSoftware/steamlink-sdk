// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/type_converters.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

using blink::mojom::NotificationDirection;
using content::PlatformNotificationData;

namespace mojo {

PlatformNotificationData
TypeConverter<PlatformNotificationData, blink::mojom::NotificationPtr>::Convert(
    const blink::mojom::NotificationPtr& notification) {
  PlatformNotificationData notification_data;

  notification_data.title = base::UTF8ToUTF16(notification->title.get());

  switch (notification->direction) {
    case NotificationDirection::LEFT_TO_RIGHT:
      notification_data.direction =
          PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
      break;
    case NotificationDirection::RIGHT_TO_LEFT:
      notification_data.direction =
          PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
      break;
    case NotificationDirection::AUTO:
      notification_data.direction = PlatformNotificationData::DIRECTION_AUTO;
      break;
  }

  notification_data.lang = notification->lang;
  notification_data.body = base::UTF8ToUTF16(notification->body.get());
  notification_data.tag = notification->tag;
  notification_data.icon = GURL(notification->icon.get());
  notification_data.badge = GURL(notification->badge.get());

  for (uint32_t vibration : notification->vibration_pattern)
    notification_data.vibration_pattern.push_back(static_cast<int>(vibration));

  notification_data.timestamp = base::Time::FromJsTime(notification->timestamp);
  notification_data.renotify = notification->renotify;
  notification_data.silent = notification->silent;
  notification_data.require_interaction = notification->require_interaction;
  notification_data.data.assign(notification->data.begin(),
                                notification->data.end());

  for (const auto& action : notification->actions) {
    content::PlatformNotificationAction data_action;

    switch (action->type) {
      case blink::mojom::NotificationActionType::BUTTON:
        data_action.type = content::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
        break;
      case blink::mojom::NotificationActionType::TEXT:
        data_action.type = content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
        break;
    }

    data_action.action = action->action;
    data_action.title = base::UTF8ToUTF16(action->title.get());
    data_action.icon = GURL(action->icon.get());
    if (!action->placeholder.is_null()) {
      data_action.placeholder = base::NullableString16(
          base::UTF8ToUTF16(action->placeholder.get()), false /* is_null */);
    }

    notification_data.actions.push_back(data_action);
  }

  return notification_data;
}

blink::mojom::NotificationPtr
TypeConverter<blink::mojom::NotificationPtr, PlatformNotificationData>::Convert(
    const PlatformNotificationData& notification_data) {
  blink::mojom::NotificationPtr notification =
      blink::mojom::Notification::New();

  notification->title = base::UTF16ToUTF8(notification_data.title);

  switch (notification_data.direction) {
    case PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT:
      notification->direction = NotificationDirection::LEFT_TO_RIGHT;
      break;
    case PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT:
      notification->direction = NotificationDirection::RIGHT_TO_LEFT;
      break;
    case PlatformNotificationData::DIRECTION_AUTO:
      notification->direction = NotificationDirection::AUTO;
      break;
  }

  notification->lang = notification_data.lang;
  notification->body = base::UTF16ToUTF8(notification_data.body);
  notification->tag = notification_data.tag;
  notification->icon = notification_data.icon.spec();
  notification->badge = notification_data.badge.spec();

  for (int vibration : notification_data.vibration_pattern)
    notification->vibration_pattern.push_back(static_cast<uint32_t>(vibration));

  notification->timestamp = notification_data.timestamp.ToJsTime();
  notification->renotify = notification_data.renotify;
  notification->silent = notification_data.silent;
  notification->require_interaction = notification_data.require_interaction;
  notification->data = std::vector<int8_t>(notification_data.data.begin(),
                                           notification_data.data.end());

  for (const auto& data_action : notification_data.actions) {
    blink::mojom::NotificationActionPtr action =
        blink::mojom::NotificationAction::New();

    switch (data_action.type) {
      case content::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON:
        action->type = blink::mojom::NotificationActionType::BUTTON;
        break;
      case content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT:
        action->type = blink::mojom::NotificationActionType::TEXT;
        break;
    }

    action->action = data_action.action;
    action->title = base::UTF16ToUTF8(data_action.title);
    action->icon = data_action.icon.spec();
    if (!data_action.placeholder.is_null())
      action->placeholder = base::UTF16ToUTF8(data_action.placeholder.string());

    notification->actions.push_back(std::move(action));
  }

  return notification;
}

}  // namespace mojo
