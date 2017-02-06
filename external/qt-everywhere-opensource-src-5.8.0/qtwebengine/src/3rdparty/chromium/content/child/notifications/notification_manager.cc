// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationDelegate.h"

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

NotificationResources ToNotificationResources(
    std::unique_ptr<blink::WebNotificationResources> web_resources) {
  NotificationResources resources;
  resources.notification_icon = web_resources->icon;
  resources.badge = web_resources->badge;
  for (const auto& action_icon : web_resources->actionIcons)
    resources.action_icons.push_back(action_icon);
  return resources;
}

}  // namespace

static base::LazyInstance<base::ThreadLocalPointer<NotificationManager>>::Leaky
    g_notification_manager_tls = LAZY_INSTANCE_INITIALIZER;

NotificationManager::NotificationManager(
    ThreadSafeSender* thread_safe_sender,
    NotificationDispatcher* notification_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      notification_dispatcher_(notification_dispatcher) {
  g_notification_manager_tls.Pointer()->Set(this);
}

NotificationManager::~NotificationManager() {
  g_notification_manager_tls.Pointer()->Set(nullptr);
}

NotificationManager* NotificationManager::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    NotificationDispatcher* notification_dispatcher) {
  if (g_notification_manager_tls.Pointer()->Get())
    return g_notification_manager_tls.Pointer()->Get();

  NotificationManager* manager =
      new NotificationManager(thread_safe_sender, notification_dispatcher);
  if (CurrentWorkerId())
    WorkerThread::AddObserver(manager);
  return manager;
}

void NotificationManager::WillStopCurrentWorkerThread() {
  delete this;
}

void NotificationManager::show(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    std::unique_ptr<blink::WebNotificationResources> notification_resources,
    blink::WebNotificationDelegate* delegate) {
  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources->actionIcons.size());

  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_page_notifications_[notification_id] = delegate;
  // TODO(mkwst): This is potentially doing the wrong thing with unique
  // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
  // https://crbug.com/490074 for detail.
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_Show(
      notification_id, blink::WebStringToGURL(origin.toString()),
      ToPlatformNotificationData(notification_data),
      ToNotificationResources(std::move(notification_resources))));
}

void NotificationManager::showPersistent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    std::unique_ptr<blink::WebNotificationResources> notification_resources,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationShowCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources->actionIcons.size());

  int64_t service_worker_registration_id =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration)
          ->registration_id();

  std::unique_ptr<blink::WebNotificationShowCallbacks> owned_callbacks(
      callbacks);

  // Verify that the author-provided payload size does not exceed our limit.
  // This is an implementation-defined limit to prevent abuse of notification
  // data as a storage mechanism. A UMA histogram records the requested sizes,
  // which enables us to track how much data authors are attempting to store.
  //
  // If the size exceeds this limit, reject the showNotification() promise. This
  // is outside of the boundaries set by the specification, but it gives authors
  // an indication that something has gone wrong.
  size_t author_data_size = notification_data.data.size();

  UMA_HISTOGRAM_COUNTS_1000("Notifications.AuthorDataSize", author_data_size);

  if (author_data_size > PlatformNotificationData::kMaximumDeveloperDataSize) {
    owned_callbacks->onError();
    return;
  }

  // TODO(peter): GenerateNotificationId is more of a request id. Consider
  // renaming the method in the NotificationDispatcher if this makes sense.
  int request_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  pending_show_notification_requests_.AddWithID(owned_callbacks.release(),
                                                request_id);

  // TODO(mkwst): This is potentially doing the wrong thing with unique
  // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
  // https://crbug.com/490074 for detail.
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ShowPersistent(
      request_id, service_worker_registration_id,
      blink::WebStringToGURL(origin.toString()),
      ToPlatformNotificationData(notification_data),
      ToNotificationResources(std::move(notification_resources))));
}

void NotificationManager::getNotifications(
    const blink::WebString& filter_tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationGetCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  WebServiceWorkerRegistrationImpl* service_worker_registration_impl =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration);

  GURL origin = GURL(service_worker_registration_impl->scope()).GetOrigin();
  int64_t service_worker_registration_id =
      service_worker_registration_impl->registration_id();

  // TODO(peter): GenerateNotificationId is more of a request id. Consider
  // renaming the method in the NotificationDispatcher if this makes sense.
  int request_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  pending_get_notification_requests_.AddWithID(callbacks, request_id);

  thread_safe_sender_->Send(new PlatformNotificationHostMsg_GetNotifications(
      request_id, service_worker_registration_id, origin,
      base::UTF16ToUTF8(base::StringPiece16(filter_tag))));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
  for (auto& iter : active_page_notifications_) {
    if (iter.second != delegate)
      continue;

    thread_safe_sender_->Send(
        new PlatformNotificationHostMsg_Close(iter.first));
    active_page_notifications_.erase(iter.first);
    return;
  }

  // It should not be possible for Blink to call close() on a Notification which
  // does not exist in either the pending or active notification lists.
  NOTREACHED();
}

void NotificationManager::closePersistent(
    const blink::WebSecurityOrigin& origin,
    int64_t persistent_notification_id) {
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ClosePersistent(
      // TODO(mkwst): This is potentially doing the wrong thing with unique
      // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
      // https://crbug.com/490074 for detail.
      blink::WebStringToGURL(origin.toString()), persistent_notification_id));
}

void NotificationManager::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  for (auto& iter : active_page_notifications_) {
    if (iter.second != delegate)
      continue;

    active_page_notifications_.erase(iter.first);
    return;
  }
}

bool NotificationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationManager, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShow, OnDidShow);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShowPersistent,
                        OnDidShowPersistent)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClose, OnDidClose);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClick, OnDidClick);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidGetNotifications,
                        OnDidGetNotifications)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationManager::OnDidShow(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchShowEvent();
}

void NotificationManager::OnDidShowPersistent(int request_id, bool success) {
  blink::WebNotificationShowCallbacks* callbacks =
      pending_show_notification_requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (!callbacks)
    return;

  if (success)
    callbacks->onSuccess();
  else
    callbacks->onError();

  pending_show_notification_requests_.Remove(request_id);
}

void NotificationManager::OnDidClose(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchCloseEvent();
  active_page_notifications_.erase(iter);
}

void NotificationManager::OnDidClick(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchClickEvent();
}

void NotificationManager::OnDidGetNotifications(
    int request_id,
    const std::vector<PersistentNotificationInfo>& notification_infos) {
  blink::WebNotificationGetCallbacks* callbacks =
      pending_get_notification_requests_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  blink::WebVector<blink::WebPersistentNotificationInfo> notifications(
      notification_infos.size());

  for (size_t i = 0; i < notification_infos.size(); ++i) {
    blink::WebPersistentNotificationInfo web_notification_info;
    web_notification_info.persistentId = notification_infos[i].first;
    web_notification_info.data =
        ToWebNotificationData(notification_infos[i].second);

    notifications[i] = web_notification_info;
  }

  callbacks->onSuccess(notifications);

  pending_get_notification_requests_.Remove(request_id);
}

}  // namespace content
