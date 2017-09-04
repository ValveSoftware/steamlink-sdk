// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/type_converters.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/common/platform_notification_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification.mojom.h"

namespace content {

TEST(NotificationTypeConvertersTest, TestConversion) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My Title");
  notification_data.direction =
      PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
  notification_data.lang = "lang";
  notification_data.body = base::ASCIIToUTF16("My Body");
  notification_data.tag = "tag";
  notification_data.icon = GURL("https://example.com/icon");
  notification_data.badge = GURL("https://example.com/badge");
  notification_data.vibration_pattern.push_back(42);
  notification_data.vibration_pattern.push_back(50);
  notification_data.timestamp = base::Time::Now();
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;
  notification_data.data.push_back(45);
  notification_data.data.push_back(127);

  PlatformNotificationAction first_action;
  first_action.type = content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
  first_action.action = "action";
  first_action.title = base::ASCIIToUTF16("My Action");
  first_action.icon = GURL("https://example.com/action_icon");
  first_action.placeholder = base::NullableString16();

  PlatformNotificationAction second_action;
  second_action.type = content::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
  second_action.action = "action 2";
  second_action.title = base::ASCIIToUTF16("My Action 2");
  second_action.icon = GURL("https://example.com/action_icon_2");
  second_action.placeholder =
      base::NullableString16(base::ASCIIToUTF16("Placeholder"), false);

  notification_data.actions.push_back(first_action);
  notification_data.actions.push_back(second_action);

  blink::mojom::NotificationPtr notification =
      blink::mojom::Notification::From(notification_data);
  PlatformNotificationData copied_notification_data =
      notification.To<PlatformNotificationData>();

  EXPECT_EQ(notification_data.title, copied_notification_data.title);
  EXPECT_EQ(notification_data.direction, copied_notification_data.direction);
  EXPECT_EQ(notification_data.lang, copied_notification_data.lang);
  EXPECT_EQ(notification_data.body, copied_notification_data.body);
  EXPECT_EQ(notification_data.tag, copied_notification_data.tag);
  EXPECT_EQ(notification_data.icon, copied_notification_data.icon);
  EXPECT_EQ(notification_data.badge, copied_notification_data.badge);
  EXPECT_EQ(notification_data.vibration_pattern,
            copied_notification_data.vibration_pattern);
  EXPECT_EQ(notification_data.timestamp, copied_notification_data.timestamp);
  EXPECT_EQ(notification_data.renotify, copied_notification_data.renotify);
  EXPECT_EQ(notification_data.silent, copied_notification_data.silent);
  EXPECT_EQ(notification_data.require_interaction,
            copied_notification_data.require_interaction);
  EXPECT_EQ(notification_data.data, copied_notification_data.data);

  ASSERT_EQ(2u, notification_data.actions.size());
  ASSERT_EQ(notification_data.actions.size(),
            copied_notification_data.actions.size());

  const PlatformNotificationAction& copied_first_action =
      copied_notification_data.actions[0];

  EXPECT_EQ(first_action.type, copied_first_action.type);
  EXPECT_EQ(first_action.action, copied_first_action.action);
  EXPECT_EQ(first_action.title, copied_first_action.title);
  EXPECT_EQ(first_action.icon, copied_first_action.icon);
  EXPECT_EQ(first_action.placeholder, copied_first_action.placeholder);

  const PlatformNotificationAction& copied_second_action =
      copied_notification_data.actions[1];

  EXPECT_EQ(second_action.type, copied_second_action.type);
  EXPECT_EQ(second_action.action, copied_second_action.action);
  EXPECT_EQ(second_action.title, copied_second_action.title);
  EXPECT_EQ(second_action.icon, copied_second_action.icon);
  EXPECT_EQ(second_action.placeholder, copied_second_action.placeholder);
}

}  // namespace content
