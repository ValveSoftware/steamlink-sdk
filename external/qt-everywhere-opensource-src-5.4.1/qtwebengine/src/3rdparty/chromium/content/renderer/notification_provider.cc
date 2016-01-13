// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_provider.h"

#include "base/strings/string_util.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/frame_messages.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

using blink::WebDocument;
using blink::WebNotification;
using blink::WebNotificationPresenter;
using blink::WebNotificationPermissionCallback;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebUserGestureIndicator;

namespace content {


NotificationProvider::NotificationProvider(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

NotificationProvider::~NotificationProvider() {
}

bool NotificationProvider::show(const WebNotification& notification) {
  WebDocument document = render_frame()->GetWebFrame()->document();
  int notification_id = manager_.RegisterNotification(notification);

  ShowDesktopNotificationHostMsgParams params;
  params.origin = GURL(document.securityOrigin().toString());
  params.icon_url = notification.iconURL();
  params.title = notification.title();
  params.body = notification.body();
  params.direction = notification.direction();
  params.replace_id = notification.replaceId();
  return Send(new DesktopNotificationHostMsg_Show(
      routing_id(), notification_id, params));
}

void NotificationProvider::cancel(const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user.
  if (id_found)
    Send(new DesktopNotificationHostMsg_Cancel(routing_id(), id));
}

void NotificationProvider::objectDestroyed(
    const WebNotification& notification) {
  int id;
  bool id_found = manager_.GetId(notification, id);
  // Won't be found if the notification has already been closed by the user.
  if (id_found)
    manager_.UnregisterNotification(id);
}

WebNotificationPresenter::Permission NotificationProvider::checkPermission(
    const WebSecurityOrigin& origin) {
  int permission;
  Send(new DesktopNotificationHostMsg_CheckPermission(
          routing_id(),
          GURL(origin.toString()),
          &permission));
  return static_cast<WebNotificationPresenter::Permission>(permission);
}

void NotificationProvider::requestPermission(
    const WebSecurityOrigin& origin,
    WebNotificationPermissionCallback* callback) {
  int id = manager_.RegisterPermissionRequest(callback);

  Send(new DesktopNotificationHostMsg_RequestPermission(
      routing_id(), GURL(origin.toString()), id));
}

bool NotificationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationProvider, message)
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostDisplay, OnDisplay);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostError, OnError);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostClose, OnClose);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PostClick, OnClick);
    IPC_MESSAGE_HANDLER(DesktopNotificationMsg_PermissionRequestDone,
                        OnPermissionRequestComplete);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == FrameMsg_Navigate::ID)
    OnNavigate();  // Don't want to swallow the message.

  return handled;
}

void NotificationProvider::OnDisplay(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before it was actually displayed to the user.
  if (found)
    notification.dispatchDisplayEvent();
}

void NotificationProvider::OnError(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the error occurred.
  if (found)
    notification.dispatchErrorEvent(WebString());
}

void NotificationProvider::OnClose(int id, bool by_user) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was closed by the user.
  if (found) {
    notification.dispatchCloseEvent(by_user);
    manager_.UnregisterNotification(id);
  }
}

void NotificationProvider::OnClick(int id) {
  WebNotification notification;
  bool found = manager_.GetNotification(id, &notification);
  // |found| may be false if the WebNotification went out of scope in
  // the page before the associated toast was clicked on.
  if (found)
    notification.dispatchClickEvent();
}

void NotificationProvider::OnPermissionRequestComplete(int id) {
  WebNotificationPermissionCallback* callback = manager_.GetCallback(id);
  DCHECK(callback);
  callback->permissionRequestComplete();
  manager_.OnPermissionRequestComplete(id);
}

void NotificationProvider::OnNavigate() {
  manager_.Clear();
}

}  // namespace content
