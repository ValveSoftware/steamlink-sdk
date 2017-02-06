// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging/push_messaging_dispatcher.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/common/push_messaging_messages.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushError.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscription.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace content {

PushMessagingDispatcher::PushMessagingDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

PushMessagingDispatcher::~PushMessagingDispatcher() {}

bool PushMessagingDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingDispatcher, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromDocumentSuccess,
                        OnSubscribeFromDocumentSuccess)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_SubscribeFromDocumentError,
                        OnSubscribeFromDocumentError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingDispatcher::OnDestruct() {
  delete this;
}

void PushMessagingDispatcher::subscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  // If a developer provided an application server key in |options|, skip
  // fetching the manifest.
  if (options.applicationServerKey.isEmpty()) {
    RenderFrameImpl::FromRoutingID(routing_id())
        ->manifest_manager()
        ->GetManifest(base::Bind(
            &PushMessagingDispatcher::DidGetManifest, base::Unretained(this),
            service_worker_registration, options, callbacks));
  } else {
    PushSubscriptionOptions content_options;
    content_options.user_visible_only = options.userVisibleOnly;
    // Just treat the server key as a string of bytes and pass it to the push
    // service.
    content_options.sender_info = options.applicationServerKey.latin1();
    DoSubscribe(service_worker_registration, content_options, callbacks);
  }
}

void PushMessagingDispatcher::DidGetManifest(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks,
    const Manifest& manifest,
    const ManifestDebugInfo&) {
  // Get the sender_info from the manifest since it wasn't provided by
  // the caller.
  if (manifest.IsEmpty()) {
    int request_id = subscription_callbacks_.Add(callbacks);
    OnSubscribeFromDocumentError(
        request_id, PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING);
    return;
  }

  PushSubscriptionOptions content_options;
  content_options.user_visible_only = options.userVisibleOnly;
  if (!manifest.gcm_sender_id.is_null()) {
    content_options.sender_info =
        base::UTF16ToUTF8(manifest.gcm_sender_id.string());
  }

  DoSubscribe(service_worker_registration, content_options, callbacks);
}

void PushMessagingDispatcher::DoSubscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const PushSubscriptionOptions& options,
    blink::WebPushSubscriptionCallbacks* callbacks) {
  int request_id = subscription_callbacks_.Add(callbacks);
  int64_t service_worker_registration_id =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration)
          ->registration_id();

  if (options.sender_info.empty()) {
    OnSubscribeFromDocumentError(request_id,
                                 PUSH_REGISTRATION_STATUS_NO_SENDER_ID);
    return;
  }
  Send(new PushMessagingHostMsg_Subscribe(
      routing_id(), request_id, service_worker_registration_id, options));
}

void PushMessagingDispatcher::OnSubscribeFromDocumentSuccess(
    int32_t request_id,
    const GURL& endpoint,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  callbacks->onSuccess(
      base::WrapUnique(new blink::WebPushSubscription(endpoint, p256dh, auth)));

  subscription_callbacks_.Remove(request_id);
}

void PushMessagingDispatcher::OnSubscribeFromDocumentError(
    int32_t request_id,
    PushRegistrationStatus status) {
  blink::WebPushSubscriptionCallbacks* callbacks =
      subscription_callbacks_.Lookup(request_id);
  DCHECK(callbacks);

  blink::WebPushError::ErrorType error_type =
      status == PUSH_REGISTRATION_STATUS_PERMISSION_DENIED
          ? blink::WebPushError::ErrorTypePermissionDenied
          : blink::WebPushError::ErrorTypeAbort;

  callbacks->onError(blink::WebPushError(
      error_type,
      blink::WebString::fromUTF8(PushRegistrationStatusToString(status))));

  subscription_callbacks_.Remove(request_id);
}

}  // namespace content
