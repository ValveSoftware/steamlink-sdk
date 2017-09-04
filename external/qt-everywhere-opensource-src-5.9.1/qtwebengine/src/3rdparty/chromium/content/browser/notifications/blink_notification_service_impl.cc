// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include "base/logging.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Returns the implementation of the PlatformNotificationService. May be NULL.
PlatformNotificationService* Service() {
  return GetContentClient()->browser()->GetPlatformNotificationService();
}

}  // namespace

BlinkNotificationServiceImpl::BlinkNotificationServiceImpl(
    PlatformNotificationContextImpl* notification_context,
    ResourceContext* resource_context,
    int render_process_id,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request)
    : notification_context_(notification_context),
      resource_context_(resource_context),
      render_process_id_(render_process_id),
      binding_(this, std::move(request)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(notification_context_);
  DCHECK(resource_context_);

  binding_.set_connection_error_handler(
      base::Bind(&BlinkNotificationServiceImpl::OnConnectionError,
                 base::Unretained(this) /* the channel is owned by this */));
}

BlinkNotificationServiceImpl::~BlinkNotificationServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BlinkNotificationServiceImpl::GetPermissionStatus(
    const mojo::String& origin,
    const GetPermissionStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!Service()) {
    callback.Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  blink::mojom::PermissionStatus permission_status =
      Service()->CheckPermissionOnIOThread(
          resource_context_, GURL(origin.get()), render_process_id_);

  callback.Run(permission_status);
}

void BlinkNotificationServiceImpl::OnConnectionError() {
  notification_context_->RemoveService(this);
  // |this| has now been deleted.
}

}  // namespace content
