// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
#define CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_

#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/active_notification_tracker.h"
#include "third_party/WebKit/public/web/WebNotification.h"
#include "third_party/WebKit/public/web/WebNotificationPresenter.h"

namespace blink {
class WebNotificationPermissionCallback;
}

namespace content {

// NotificationProvider class is owned by the RenderFrame.  Only to be used on
// the main thread.
class NotificationProvider : public RenderFrameObserver,
                             public blink::WebNotificationPresenter {
 public:
  explicit NotificationProvider(RenderFrame* render_frame);
  virtual ~NotificationProvider();

 private:
  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // blink::WebNotificationPresenter interface.
  virtual bool show(const blink::WebNotification& proxy);
  virtual void cancel(const blink::WebNotification& proxy);
  virtual void objectDestroyed(const blink::WebNotification& proxy);
  virtual blink::WebNotificationPresenter::Permission checkPermission(
      const blink::WebSecurityOrigin& origin);
  virtual void requestPermission(const blink::WebSecurityOrigin& origin,
      blink::WebNotificationPermissionCallback* callback);

  // IPC handlers.
  void OnDisplay(int id);
  void OnError(int id);
  void OnClose(int id, bool by_user);
  void OnClick(int id);
  void OnPermissionRequestComplete(int id);
  void OnNavigate();

  // A tracker object which manages the active notifications and the IDs
  // that are used to refer to them over IPC.
  ActiveNotificationTracker manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATION_PROVIDER_H_
