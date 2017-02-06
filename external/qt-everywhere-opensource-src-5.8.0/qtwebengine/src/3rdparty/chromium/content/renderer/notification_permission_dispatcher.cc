// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_permission_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "content/public/renderer/render_frame.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/modules/notifications/WebNotificationPermissionCallback.h"

using blink::WebNotificationPermissionCallback;

namespace content {

NotificationPermissionDispatcher::NotificationPermissionDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

NotificationPermissionDispatcher::~NotificationPermissionDispatcher() {}

void NotificationPermissionDispatcher::RequestPermission(
    const blink::WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback) {
  if (!permission_service_.get()) {
    render_frame()->GetRemoteInterfaces()->GetInterface(&permission_service_);
  }

  std::unique_ptr<WebNotificationPermissionCallback> owned_callback(callback);

  // base::Unretained is safe here because the Mojo channel, with associated
  // callbacks, will be deleted before the "this" instance is deleted.
  permission_service_->RequestPermission(
      blink::mojom::PermissionName::NOTIFICATIONS, origin.toString().utf8(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&NotificationPermissionDispatcher::OnPermissionRequestComplete,
                 base::Unretained(this),
                 base::Passed(std::move(owned_callback))));
}

void NotificationPermissionDispatcher::OnPermissionRequestComplete(
    std::unique_ptr<WebNotificationPermissionCallback> callback,
    blink::mojom::PermissionStatus status) {
  DCHECK(callback);

  blink::mojom::blink::PermissionStatus blink_status =
      static_cast<blink::mojom::blink::PermissionStatus>(status);

  callback->permissionRequestComplete(blink_status);
}

void NotificationPermissionDispatcher::OnDestruct() {
  delete this;
}

}  // namespace content
