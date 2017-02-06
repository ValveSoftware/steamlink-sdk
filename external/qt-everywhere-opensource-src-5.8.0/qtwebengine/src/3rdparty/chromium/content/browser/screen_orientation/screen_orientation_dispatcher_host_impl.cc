// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host_impl.h"

#include "content/common/screen_orientation_messages.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/screen_orientation_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

namespace content {

ScreenOrientationDispatcherHostImpl::LockInformation::LockInformation(
    int request_id, int process_id, int routing_id)
    : request_id(request_id),
      process_id(process_id),
      routing_id(routing_id) {
}

ScreenOrientationDispatcherHostImpl::ScreenOrientationDispatcherHostImpl(
    WebContents* web_contents)
  : WebContentsObserver(web_contents),
    current_lock_(NULL) {
  provider_.reset(new ScreenOrientationProvider(this, web_contents));
}

ScreenOrientationDispatcherHostImpl::~ScreenOrientationDispatcherHostImpl() {
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHostImpl::ResetCurrentLock() {
  if (current_lock_) {
    delete current_lock_;
    current_lock_ = 0;
  }
}

bool ScreenOrientationDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ScreenOrientationDispatcherHostImpl, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_LockRequest, OnLockRequest)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_Unlock, OnUnlockRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ScreenOrientationDispatcherHostImpl::DidNavigateMainFrame(
    const LoadCommittedDetails& details, const FrameNavigateParams& params) {
  if (!provider_ || details.is_in_page)
    return;
  provider_->UnlockOrientation();
}

RenderFrameHost*
ScreenOrientationDispatcherHostImpl::GetRenderFrameHostForRequestID(
    int request_id) {
  if (!current_lock_ || current_lock_->request_id != request_id)
    return NULL;

  return RenderFrameHost::FromID(current_lock_->process_id,
                                 current_lock_->routing_id);
}

void ScreenOrientationDispatcherHostImpl::NotifyLockSuccess(int request_id) {
  RenderFrameHost* render_frame_host =
      GetRenderFrameHostForRequestID(request_id);
  if (!render_frame_host)
    return;

  render_frame_host->Send(new ScreenOrientationMsg_LockSuccess(
      render_frame_host->GetRoutingID(), request_id));
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHostImpl::NotifyLockError(
    int request_id, blink::WebLockOrientationError error) {
  RenderFrameHost* render_frame_host =
      GetRenderFrameHostForRequestID(request_id);
  if (!render_frame_host)
    return;

  NotifyLockError(request_id, render_frame_host, error);
}

void ScreenOrientationDispatcherHostImpl::NotifyLockError(
    int request_id,
    RenderFrameHost* render_frame_host,
    blink::WebLockOrientationError error) {
  render_frame_host->Send(new ScreenOrientationMsg_LockError(
      render_frame_host->GetRoutingID(), request_id, error));
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHostImpl::OnOrientationChange() {
  if (provider_)
    provider_->OnOrientationChange();
}

void ScreenOrientationDispatcherHostImpl::OnLockRequest(
    RenderFrameHost* render_frame_host,
    blink::WebScreenOrientationLockType orientation,
    int request_id) {
  if (current_lock_) {
    NotifyLockError(current_lock_->request_id, render_frame_host,
                    blink::WebLockOrientationErrorCanceled);
  }

  if (!provider_) {
    NotifyLockError(request_id, render_frame_host,
                    blink::WebLockOrientationErrorNotAvailable);
    return;
  }

  current_lock_ = new LockInformation(request_id,
                                      render_frame_host->GetProcess()->GetID(),
                                      render_frame_host->GetRoutingID());

  provider_->LockOrientation(request_id, orientation);
}

void ScreenOrientationDispatcherHostImpl::OnUnlockRequest(
    RenderFrameHost* render_frame_host) {
  if (current_lock_) {
    NotifyLockError(current_lock_->request_id, render_frame_host,
                    blink::WebLockOrientationErrorCanceled);
  }

  if (!provider_)
    return;

  provider_->UnlockOrientation();
}

}  // namespace content
