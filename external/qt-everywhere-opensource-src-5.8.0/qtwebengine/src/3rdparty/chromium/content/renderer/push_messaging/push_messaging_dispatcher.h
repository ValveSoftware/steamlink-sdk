// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "content/public/common/push_messaging_status.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushClient.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

class GURL;

namespace blink {
struct WebPushSubscriptionOptions;
}

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

struct Manifest;
struct ManifestDebugInfo;
struct PushSubscriptionOptions;

class PushMessagingDispatcher : public RenderFrameObserver,
                                public blink::WebPushClient {
 public:
  explicit PushMessagingDispatcher(RenderFrame* render_frame);
  ~PushMessagingDispatcher() override;

 private:
  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  // WebPushClient implementation.
  void subscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks) override;

  void DidGetManifest(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks,
      const Manifest& manifest,
      const ManifestDebugInfo&);

  void DoSubscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const PushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks);

  void OnSubscribeFromDocumentSuccess(int32_t request_id,
                                      const GURL& endpoint,
                                      const std::vector<uint8_t>& p256dh,
                                      const std::vector<uint8_t>& auth);

  void OnSubscribeFromDocumentError(int32_t request_id,
                                    PushRegistrationStatus status);

  IDMap<blink::WebPushSubscriptionCallbacks, IDMapOwnPointer>
      subscription_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
