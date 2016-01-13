// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging_dispatcher.h"

#include "content/common/push_messaging_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebPushError.h"
#include "third_party/WebKit/public/platform/WebPushRegistration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

using blink::WebString;

namespace content {

PushMessagingDispatcher::PushMessagingDispatcher(RenderViewImpl* render_view)
    : RenderViewObserver(render_view) {}

PushMessagingDispatcher::~PushMessagingDispatcher() {}

bool PushMessagingDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingDispatcher, message)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterSuccess, OnRegisterSuccess)
    IPC_MESSAGE_HANDLER(PushMessagingMsg_RegisterError, OnRegisterError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushMessagingDispatcher::registerPushMessaging(
    const WebString& sender_id,
    blink::WebPushRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  int callbacks_id = registration_callbacks_.Add(callbacks);
  Send(new PushMessagingHostMsg_Register(
      routing_id(), callbacks_id, sender_id.utf8()));
}

void PushMessagingDispatcher::OnRegisterSuccess(
    int32 callbacks_id,
    const GURL& endpoint,
    const std::string& registration_id) {
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushRegistration> registration(
      new blink::WebPushRegistration(
          WebString::fromUTF8(endpoint.spec()),
          WebString::fromUTF8(registration_id)));
  callbacks->onSuccess(registration.release());
  registration_callbacks_.Remove(callbacks_id);
}

void PushMessagingDispatcher::OnRegisterError(int32 callbacks_id) {
  const std::string kAbortErrorReason = "Registration failed.";
  blink::WebPushRegistrationCallbacks* callbacks =
      registration_callbacks_.Lookup(callbacks_id);
  CHECK(callbacks);

  scoped_ptr<blink::WebPushError> error(
      new blink::WebPushError(
          blink::WebPushError::ErrorTypeAbort,
          WebString::fromUTF8(kAbortErrorReason)));
  callbacks->onError(error.release());
  registration_callbacks_.Remove(callbacks_id);
}

}  // namespace content
