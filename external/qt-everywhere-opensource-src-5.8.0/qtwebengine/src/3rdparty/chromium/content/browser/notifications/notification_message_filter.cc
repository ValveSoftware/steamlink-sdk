// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_message_filter.h"

#include <utility>

#include "base/callback.h"
#include "content/browser/bad_message.h"
#include "content/browser/notifications/page_notification_delegate.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationConstants.h"

namespace content {

namespace {

const int kMinimumVibrationDurationMs = 1;      // 1 millisecond
const int kMaximumVibrationDurationMs = 10000;  // 10 seconds

PlatformNotificationData SanitizeNotificationData(
    const PlatformNotificationData& notification_data) {
  PlatformNotificationData sanitized_data = notification_data;

  // Make sure that the vibration values are within reasonable bounds.
  for (int& pattern : sanitized_data.vibration_pattern) {
    pattern = std::min(kMaximumVibrationDurationMs,
                       std::max(kMinimumVibrationDurationMs, pattern));
  }

  // Ensure there aren't more actions than supported.
  if (sanitized_data.actions.size() > blink::kWebNotificationMaxActions)
    sanitized_data.actions.resize(blink::kWebNotificationMaxActions);

  return sanitized_data;
}

// Returns true when |resources| looks ok, false otherwise.
bool ValidateNotificationResources(const NotificationResources& resources) {
  if (resources.notification_icon.width() >
          blink::kWebNotificationMaxIconSizePx ||
      resources.notification_icon.height() >
          blink::kWebNotificationMaxIconSizePx) {
    return false;
  }
  if (resources.badge.width() > blink::kWebNotificationMaxBadgeSizePx ||
      resources.badge.height() > blink::kWebNotificationMaxBadgeSizePx) {
    return false;
  }
  for (const auto& action_icon : resources.action_icons) {
    if (action_icon.width() > blink::kWebNotificationMaxActionIconSizePx ||
        action_icon.height() > blink::kWebNotificationMaxActionIconSizePx) {
      return false;
    }
  }
  return true;
}

}  // namespace

NotificationMessageFilter::NotificationMessageFilter(
    int process_id,
    PlatformNotificationContextImpl* notification_context,
    ResourceContext* resource_context,
    BrowserContext* browser_context)
    : BrowserMessageFilter(PlatformNotificationMsgStart),
      process_id_(process_id),
      notification_context_(notification_context),
      resource_context_(resource_context),
      browser_context_(browser_context),
      weak_factory_io_(this) {}

NotificationMessageFilter::~NotificationMessageFilter() {}

void NotificationMessageFilter::DidCloseNotification(int notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool NotificationMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationMessageFilter, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Show,
                        OnShowPlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ShowPersistent,
                        OnShowPersistentNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_GetNotifications,
                        OnGetNotifications)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Close,
                        OnClosePlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ClosePersistent,
                        OnClosePersistentNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == PlatformNotificationHostMsg_Show::ID ||
      message.type() == PlatformNotificationHostMsg_Close::ID)
    *thread = BrowserThread::UI;
}

void NotificationMessageFilter::OnShowPlatformNotification(
    int notification_id,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  if (!ValidateNotificationResources(notification_resources)) {
    bad_message::ReceivedBadMessage(this, bad_message::NMF_INVALID_ARGUMENT);
    return;
  }

  std::unique_ptr<DesktopNotificationDelegate> delegate(
      new PageNotificationDelegate(process_id_, notification_id));

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  if (!VerifyNotificationPermissionGranted(service, origin))
    return;

  base::Closure close_closure;
  service->DisplayNotification(
      browser_context_, origin, SanitizeNotificationData(notification_data),
      notification_resources, std::move(delegate), &close_closure);

  if (!close_closure.is_null())
    close_closures_[notification_id] = close_closure;
}

void NotificationMessageFilter::OnShowPersistentNotification(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetPermissionForOriginOnIO(origin) !=
      blink::mojom::PermissionStatus::GRANTED) {
    bad_message::ReceivedBadMessage(this, bad_message::NMF_NO_PERMISSION_SHOW);
    return;
  }

  if (!ValidateNotificationResources(notification_resources)) {
    bad_message::ReceivedBadMessage(this, bad_message::NMF_INVALID_ARGUMENT);
    return;
  }

  NotificationDatabaseData database_data;
  database_data.origin = origin;
  database_data.service_worker_registration_id = service_worker_registration_id;

  PlatformNotificationData sanitized_notification_data =
      SanitizeNotificationData(notification_data);
  database_data.notification_data = sanitized_notification_data;

  // TODO(peter): Significantly reduce the amount of information we need to
  // retain outside of the database for displaying notifications.
  notification_context_->WriteNotificationData(
      origin, database_data,
      base::Bind(&NotificationMessageFilter::DidWritePersistentNotificationData,
                 weak_factory_io_.GetWeakPtr(), request_id, origin,
                 sanitized_notification_data, notification_resources));
}

void NotificationMessageFilter::DidWritePersistentNotificationData(
    int request_id,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const NotificationResources& notification_resources,
    bool success,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (success) {
    PlatformNotificationService* service =
        GetContentClient()->browser()->GetPlatformNotificationService();
    DCHECK(service);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PlatformNotificationService::DisplayPersistentNotification,
                   base::Unretained(service),  // The service is a singleton.
                   browser_context_, persistent_notification_id, origin,
                   notification_data, notification_resources));
  }

  Send(new PlatformNotificationMsg_DidShowPersistent(request_id, success));
}

void NotificationMessageFilter::OnGetNotifications(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& filter_tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetPermissionForOriginOnIO(origin) !=
      blink::mojom::PermissionStatus::GRANTED) {
    // No permission has been granted for the given origin. It is harmless to
    // try to get notifications without permission, so return an empty vector
    // indicating that no (accessible) notifications exist at this time.
    Send(new PlatformNotificationMsg_DidGetNotifications(
        request_id, std::vector<PersistentNotificationInfo>()));
    return;
  }

  notification_context_->ReadAllNotificationDataForServiceWorkerRegistration(
      origin, service_worker_registration_id,
      base::Bind(&NotificationMessageFilter::DidGetNotifications,
                 weak_factory_io_.GetWeakPtr(), request_id, filter_tag));
}

void NotificationMessageFilter::DidGetNotifications(
    int request_id,
    const std::string& filter_tag,
    bool success,
    const std::vector<NotificationDatabaseData>& notifications) {
  std::vector<PersistentNotificationInfo> persistent_notifications;
  for (const NotificationDatabaseData& database_data : notifications) {
    if (!filter_tag.empty()) {
      const std::string& tag = database_data.notification_data.tag;
      if (tag != filter_tag)
        continue;
    }

    persistent_notifications.push_back(std::make_pair(
        database_data.notification_id, database_data.notification_data));
  }

  Send(new PlatformNotificationMsg_DidGetNotifications(
      request_id, persistent_notifications));
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderProcessHost::FromID(process_id_))
    return;

  if (!close_closures_.count(notification_id))
    return;

  close_closures_[notification_id].Run();
  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnClosePersistentNotification(
    const GURL& origin,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetPermissionForOriginOnIO(origin) !=
      blink::mojom::PermissionStatus::GRANTED) {
    bad_message::ReceivedBadMessage(this, bad_message::NMF_NO_PERMISSION_CLOSE);
    return;
  }

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  // There's no point in waiting until the database data has been removed before
  // closing the notification presented to the user. Post that task immediately.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PlatformNotificationService::ClosePersistentNotification,
                 base::Unretained(service),  // The service is a singleton.
                 browser_context_, persistent_notification_id));

  notification_context_->DeleteNotificationData(
      persistent_notification_id, origin,
      base::Bind(
          &NotificationMessageFilter::DidDeletePersistentNotificationData,
          weak_factory_io_.GetWeakPtr()));
}

void NotificationMessageFilter::DidDeletePersistentNotificationData(
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(peter): Consider feeding back to the renderer that the notification
  // has been closed.
}

blink::mojom::PermissionStatus
NotificationMessageFilter::GetPermissionForOriginOnIO(
    const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (!service)
    return blink::mojom::PermissionStatus::DENIED;

  return service->CheckPermissionOnIOThread(resource_context_, origin,
                                            process_id_);
}

bool NotificationMessageFilter::VerifyNotificationPermissionGranted(
    PlatformNotificationService* service,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::mojom::PermissionStatus permission_status =
      service->CheckPermissionOnUIThread(browser_context_, origin, process_id_);

  if (permission_status == blink::mojom::PermissionStatus::GRANTED)
    return true;

  bad_message::ReceivedBadMessage(this, bad_message::NMF_NO_PERMISSION_VERIFY);
  return false;
}

}  // namespace content
