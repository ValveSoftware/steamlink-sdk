// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_database_data.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

class GURL;

namespace content {

class BrowserContext;
struct NotificationResources;
class PlatformNotificationContextImpl;
struct PlatformNotificationData;
class PlatformNotificationService;
class ResourceContext;

class NotificationMessageFilter : public BrowserMessageFilter {
 public:
  NotificationMessageFilter(
      int process_id,
      PlatformNotificationContextImpl* notification_context,
      ResourceContext* resource_context,
      BrowserContext* browser_context);

  // To be called by the notification's delegate when it has closed, so that
  // the close closure associated with that notification can be removed.
  void DidCloseNotification(int notification_id);

  // BrowserMessageFilter implementation. Called on the UI thread.
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

 protected:
  ~NotificationMessageFilter() override;

 private:
  friend class base::DeleteHelper<NotificationMessageFilter>;
  friend class BrowserThread;

  void OnShowPlatformNotification(
      int notification_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources);
  void OnShowPersistentNotification(
      int request_id,
      int64_t service_worker_registration_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources);
  void OnGetNotifications(int request_id,
                          int64_t service_worker_registration_id,
                          const GURL& origin,
                          const std::string& filter_tag);
  void OnClosePlatformNotification(int notification_id);
  void OnClosePersistentNotification(const GURL& origin,
                                     int64_t persistent_notification_id);

  // Callback to be invoked by the notification context when the notification
  // data for the persistent notification may have been written, as indicated by
  // |success|. Will present the notification to the user when successful.
  void DidWritePersistentNotificationData(
      int request_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources,
      bool success,
      int64_t persistent_notification_id);

  // Callback to be invoked when all notifications belonging to a Service Worker
  // registration have been read from the database. The |success| argument
  // indicates whether the data could be read successfully, whereas the actual
  // notifications will be stored in |notifications|.
  void DidGetNotifications(
      int request_id,
      const std::string& filter_tag,
      bool success,
      const std::vector<NotificationDatabaseData>& notifications);

  // Callback to be invoked when the data associated with a persistent
  // notification has been removed by the database, unless an error occurred,
  // which will be indicated by |success|.
  void DidDeletePersistentNotificationData(bool success);

  // Returns the permission status for |origin|. Must only be used on the IO
  // thread. If the PlatformNotificationService is unavailable, permission will
  // assumed to be denied.
  blink::mojom::PermissionStatus GetPermissionForOriginOnIO(
      const GURL& origin) const;

  // Verifies that Web Notification permission has been granted for |origin| in
  // cases where the renderer shouldn't send messages if it weren't the case. If
  // no permission has been granted, a bad message has been received and the
  // renderer should be killed accordingly.
  bool VerifyNotificationPermissionGranted(PlatformNotificationService* service,
                                           const GURL& origin);

  int process_id_;
  scoped_refptr<PlatformNotificationContextImpl> notification_context_;
  ResourceContext* resource_context_;
  BrowserContext* browser_context_;

  // Map mapping notification ids to their associated close closures.
  std::map<int, base::Closure> close_closures_;

  base::WeakPtrFactory<NotificationMessageFilter> weak_factory_io_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
