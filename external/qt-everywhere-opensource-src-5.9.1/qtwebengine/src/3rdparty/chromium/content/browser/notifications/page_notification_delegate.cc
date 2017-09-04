// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/page_notification_delegate.h"

#include "content/browser/notifications/notification_message_filter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/render_process_host.h"

namespace content {

PageNotificationDelegate::PageNotificationDelegate(
    int render_process_id,
    int non_persistent_notification_id,
    const std::string& notification_id)
    : render_process_id_(render_process_id),
      non_persistent_notification_id_(non_persistent_notification_id),
      notification_id_(notification_id) {}

PageNotificationDelegate::~PageNotificationDelegate() {}

void PageNotificationDelegate::NotificationDisplayed() {
  RenderProcessHost* sender = RenderProcessHost::FromID(render_process_id_);
  if (!sender)
    return;

  sender->Send(
      new PlatformNotificationMsg_DidShow(non_persistent_notification_id_));
}

void PageNotificationDelegate::NotificationClosed() {
  RenderProcessHost* sender = RenderProcessHost::FromID(render_process_id_);
  if (!sender)
    return;

  sender->Send(
      new PlatformNotificationMsg_DidClose(non_persistent_notification_id_));

  static_cast<RenderProcessHostImpl*>(sender)
      ->notification_message_filter()
      ->DidCloseNotification(notification_id_);
}

void PageNotificationDelegate::NotificationClick() {
  RenderProcessHost* sender = RenderProcessHost::FromID(render_process_id_);
  if (!sender)
    return;

  sender->Send(
      new PlatformNotificationMsg_DidClick(non_persistent_notification_id_));
}

}  // namespace content
