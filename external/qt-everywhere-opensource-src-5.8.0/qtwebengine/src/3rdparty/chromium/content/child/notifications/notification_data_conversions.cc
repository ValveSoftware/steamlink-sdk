// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_data_conversions.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationAction.h"

using blink::WebNotificationData;

namespace content {

PlatformNotificationData ToPlatformNotificationData(
    const WebNotificationData& web_data) {
  PlatformNotificationData platform_data;
  platform_data.title = web_data.title;

  switch (web_data.direction) {
    case WebNotificationData::DirectionLeftToRight:
      platform_data.direction =
          PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
      break;
    case WebNotificationData::DirectionRightToLeft:
      platform_data.direction =
          PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
      break;
    case WebNotificationData::DirectionAuto:
      platform_data.direction = PlatformNotificationData::DIRECTION_AUTO;
      break;
  }

  platform_data.lang = base::UTF16ToUTF8(base::StringPiece16(web_data.lang));
  platform_data.body = web_data.body;
  platform_data.tag = base::UTF16ToUTF8(base::StringPiece16(web_data.tag));
  platform_data.icon = blink::WebStringToGURL(web_data.icon.string());
  platform_data.badge = blink::WebStringToGURL(web_data.badge.string());
  platform_data.vibration_pattern.assign(web_data.vibrate.begin(),
                                         web_data.vibrate.end());
  platform_data.timestamp = base::Time::FromJsTime(web_data.timestamp);
  platform_data.renotify = web_data.renotify;
  platform_data.silent = web_data.silent;
  platform_data.require_interaction = web_data.requireInteraction;
  platform_data.data.assign(web_data.data.begin(), web_data.data.end());
  platform_data.actions.resize(web_data.actions.size());
  for (size_t i = 0; i < web_data.actions.size(); ++i) {
    switch (web_data.actions[i].type) {
      case blink::WebNotificationAction::Button:
        platform_data.actions[i].type =
            PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
        break;
      case blink::WebNotificationAction::Text:
        platform_data.actions[i].type = PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
        break;
      default:
        NOTREACHED() << "Unknown notification action type: "
                     << web_data.actions[i].type;
    }
    platform_data.actions[i].action =
        base::UTF16ToUTF8(base::StringPiece16(web_data.actions[i].action));
    platform_data.actions[i].title = web_data.actions[i].title;
    platform_data.actions[i].icon =
        blink::WebStringToGURL(web_data.actions[i].icon.string());
    platform_data.actions[i].placeholder = web_data.actions[i].placeholder;
  }

  return platform_data;
}

WebNotificationData ToWebNotificationData(
    const PlatformNotificationData& platform_data) {
  WebNotificationData web_data;
  web_data.title = platform_data.title;

  switch (platform_data.direction) {
    case PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT:
      web_data.direction = WebNotificationData::DirectionLeftToRight;
      break;
    case PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT:
      web_data.direction = WebNotificationData::DirectionRightToLeft;
      break;
    case PlatformNotificationData::DIRECTION_AUTO:
      web_data.direction = WebNotificationData::DirectionAuto;
      break;
  }

  web_data.lang = blink::WebString::fromUTF8(platform_data.lang);
  web_data.body = platform_data.body;
  web_data.tag = blink::WebString::fromUTF8(platform_data.tag);
  web_data.icon = blink::WebURL(platform_data.icon);
  web_data.badge = blink::WebURL(platform_data.badge);
  web_data.vibrate = platform_data.vibration_pattern;
  web_data.timestamp = platform_data.timestamp.ToJsTime();
  web_data.renotify = platform_data.renotify;
  web_data.silent = platform_data.silent;
  web_data.requireInteraction = platform_data.require_interaction;
  web_data.data = platform_data.data;
  blink::WebVector<blink::WebNotificationAction> resized(
      platform_data.actions.size());
  web_data.actions.swap(resized);
  for (size_t i = 0; i < platform_data.actions.size(); ++i) {
    switch (platform_data.actions[i].type) {
      case PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON:
        web_data.actions[i].type = blink::WebNotificationAction::Button;
        break;
      case PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT:
        web_data.actions[i].type = blink::WebNotificationAction::Text;
        break;
      default:
        NOTREACHED() << "Unknown platform data type: "
                     << platform_data.actions[i].type;
    }
    web_data.actions[i].action =
        blink::WebString::fromUTF8(platform_data.actions[i].action);
    web_data.actions[i].title = platform_data.actions[i].title;
    web_data.actions[i].icon = blink::WebURL(platform_data.actions[i].icon);
    web_data.actions[i].placeholder = platform_data.actions[i].placeholder;
  }

  return web_data;
}

}  // namespace content
