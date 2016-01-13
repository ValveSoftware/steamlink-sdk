// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"

#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

ScreenOrientationDispatcherHost::ScreenOrientationDispatcherHost(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  if (!provider_.get())
    provider_.reset(CreateProvider());
}

ScreenOrientationDispatcherHost::~ScreenOrientationDispatcherHost() {
}

bool ScreenOrientationDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ScreenOrientationDispatcherHost, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_LockRequest, OnLockRequest)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_Unlock, OnUnlockRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ScreenOrientationDispatcherHost::OnOrientationChange(
    blink::WebScreenOrientationType orientation) {
  Send(new ScreenOrientationMsg_OrientationChange(orientation));
}

void ScreenOrientationDispatcherHost::SetProviderForTests(
    ScreenOrientationProvider* provider) {
  provider_.reset(provider);
}

void ScreenOrientationDispatcherHost::OnLockRequest(
    RenderFrameHost* render_frame_host,
    blink::WebScreenOrientationLockType orientation,
    int request_id) {
  if (!provider_) {
    render_frame_host->Send(new ScreenOrientationMsg_LockError(
        render_frame_host->GetRoutingID(),
        request_id,
        blink::WebLockOrientationCallback::ErrorTypeNotAvailable));
    return;
  }

  // TODO(mlamouri): pass real values.
  render_frame_host->Send(new ScreenOrientationMsg_LockSuccess(
      render_frame_host->GetRoutingID(),
      request_id,
      0,
      blink::WebScreenOrientationPortraitPrimary));
  provider_->LockOrientation(orientation);
}

void ScreenOrientationDispatcherHost::OnUnlockRequest(
    RenderFrameHost* render_frame_host) {
  if (!provider_.get())
    return;

  provider_->UnlockOrientation();
}

#if !defined(OS_ANDROID)
// static
ScreenOrientationProvider* ScreenOrientationDispatcherHost::CreateProvider() {
  return NULL;
}
#endif

}  // namespace content
