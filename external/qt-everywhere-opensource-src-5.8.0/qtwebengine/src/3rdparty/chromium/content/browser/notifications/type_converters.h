// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_NOTIFICATIONS_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification.mojom.h"

namespace mojo {

// TODO(peter): Remove these converters together with the entire
// PlatformNotificationData type.

template <>
struct TypeConverter<content::PlatformNotificationData,
                     blink::mojom::NotificationPtr> {
  static CONTENT_EXPORT content::PlatformNotificationData Convert(
      const blink::mojom::NotificationPtr& notification);
};

template <>
struct TypeConverter<blink::mojom::NotificationPtr,
                     content::PlatformNotificationData> {
  static CONTENT_EXPORT blink::mojom::NotificationPtr Convert(
      const content::PlatformNotificationData& notification_data);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_NOTIFICATIONS_TYPE_CONVERTERS_H_
