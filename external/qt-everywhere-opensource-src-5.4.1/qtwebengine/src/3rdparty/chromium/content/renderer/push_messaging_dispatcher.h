// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_

#include <string>

#include "base/id_map.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebPushClient.h"

class GURL;

namespace IPC {
class Message;
}  // namespace IPC

namespace blink {
class WebString;
}  // namespace blink

namespace content {
class RenderViewImpl;

class PushMessagingDispatcher : public RenderViewObserver,
                                public blink::WebPushClient {
 public:
  explicit PushMessagingDispatcher(RenderViewImpl* render_view);
  virtual ~PushMessagingDispatcher();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebPushClient implementation.
  virtual void registerPushMessaging(
      const blink::WebString& sender_id,
      blink::WebPushRegistrationCallbacks* callbacks) OVERRIDE;

  void OnRegisterSuccess(int32 callbacks_id,
                         const GURL& endpoint,
                         const std::string& registration_id);

  void OnRegisterError(int32 callbacks_id);

  IDMap<blink::WebPushRegistrationCallbacks, IDMapOwnPointer>
      registration_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_
