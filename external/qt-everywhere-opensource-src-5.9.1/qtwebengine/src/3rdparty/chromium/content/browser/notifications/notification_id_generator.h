// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_

#include <stdint.h>
#include <string>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class BrowserContext;

// Generates deterministic notification ids for Web Notifications.
//
// The notification id must be deterministic for a given browser context, origin
// and tag, when the tag is non-empty, or unique for the given notification when
// the tag is empty. For non-persistent notifications, the uniqueness will be
// based on the render process id. For persistent notifications, the generated
// id will be globally unique for the lifetime of the notification database.
//
// Notifications coming from the same origin and having the same tag will result
// in the same notification id being generated. This id may then be used to
// update the notification in the platform notification service.
//
// The notification id will be used by the notification service for determining
// when to replace notifications, and as the unique identifier when a
// notification has to be closed programmatically.
//
// It is important to note that, for persistent notifications, the generated
// notification id can outlive the browser process responsible for creating it.
class CONTENT_EXPORT NotificationIdGenerator {
 public:
  explicit NotificationIdGenerator(BrowserContext* browser_context);
  ~NotificationIdGenerator();

  // Returns whether |notification_id| belongs to a persistent notification.
  static bool IsPersistentNotification(
      const base::StringPiece& notification_id);

  // Returns whether |notification_id| belongs to a non-persistent notification.
  static bool IsNonPersistentNotification(
      const base::StringPiece& notification_id);

  // Generates an id for a persistent notification given the notification's
  // origin, tag and persistent notification id. The persistent notification id
  // will have been created by the persistent notification database.
  std::string GenerateForPersistentNotification(
      const GURL& origin,
      const std::string& tag,
      int64_t persistent_notification_id) const;

  // Generates an id for a non-persistent notification given the notification's
  // origin, tag and non-persistent notification id. The non-persistent
  // notification id must've been created by the |render_process_id|.
  std::string GenerateForNonPersistentNotification(
      const GURL& origin,
      const std::string& tag,
      int non_persistent_notification_id,
      int render_process_id) const;

 private:
  // The NotificationMessageFilter that owns |this| will outlive the context.
  BrowserContext* browser_context_;
};

}  // namespace context

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_ID_GENERATOR_H_
