// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

class GURL;

namespace content {

class BrowserContext;
class DesktopNotificationDelegate;
struct NotificationResources;
struct PlatformNotificationData;
class ResourceContext;

// The service using which notifications can be presented to the user. There
// should be a unique instance of the PlatformNotificationService depending
// on the browsing context being used.
class CONTENT_EXPORT PlatformNotificationService {
 public:
  virtual ~PlatformNotificationService() {}

  // Checks if |origin| has permission to display Web Notifications.
  // This method must only be called on the UI thread.
  virtual blink::mojom::PermissionStatus CheckPermissionOnUIThread(
      BrowserContext* browser_context,
      const GURL& origin,
      int render_process_id) = 0;

  // Checks if |origin| has permission to display Web Notifications. This method
  // exists to serve the synchronous IPC required by the Notification.permission
  // JavaScript getter, and should not be used for other purposes. See
  // https://crbug.com/446497 for the plan to deprecate this method.
  // This method must only be called on the IO thread.
  virtual blink::mojom::PermissionStatus CheckPermissionOnIOThread(
      ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) = 0;

  // Displays the notification described in |notification_data| to the user. A
  // closure through which the notification can be closed will be stored in the
  // |cancel_callback| argument. This method must be called on the UI thread.
  virtual void DisplayNotification(
      BrowserContext* browser_context,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources,
      std::unique_ptr<DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) = 0;

  // Displays the persistent notification described in |notification_data| to
  // the user. This method must be called on the UI thread.
  virtual void DisplayPersistentNotification(
      BrowserContext* browser_context,
      int64_t persistent_notification_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources) = 0;

  // Closes the persistent notification identified by
  // |persistent_notification_id|. This method must be called on the UI thread.
  virtual void ClosePersistentNotification(
      BrowserContext* browser_context,
      int64_t persistent_notification_id) = 0;

  // Writes the ids of all currently displaying persistent notifications for the
  // given |browser_context| to |displayed_notifications|. Returns whether the
  // platform is able to provide such a set.
  virtual bool GetDisplayedPersistentNotifications(
      BrowserContext* browser_context,
      std::set<std::string>* displayed_notifications) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_
