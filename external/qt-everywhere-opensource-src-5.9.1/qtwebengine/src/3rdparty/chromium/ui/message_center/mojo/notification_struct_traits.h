// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_

#include "ui/message_center/mojo/notification.mojom-shared.h"
#include "ui/message_center/notification.h"

namespace mojo {

template <>
struct StructTraits<message_center::mojom::NotificationDataView,
                    message_center::Notification> {
  static const std::string& id(const message_center::Notification& n);
  static const base::string16& title(const message_center::Notification& n);
  static const base::string16& message(const message_center::Notification& n);
  static const SkBitmap& icon(const message_center::Notification& n);
  static const base::string16& display_source(
      const message_center::Notification& n);
  static const GURL& origin_url(const message_center::Notification& n);
  static bool Read(message_center::mojom::NotificationDataView data,
                   message_center::Notification* out);
};

}  // namespace mojo

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_STRUCT_TRAITS_H_
