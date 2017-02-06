// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_context.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "device/power_save_blocker/power_save_blocker.h"

namespace content {

WakeLockServiceContext::WakeLockServiceContext(WebContents* web_contents)
    : WebContentsObserver(web_contents), weak_factory_(this) {}

WakeLockServiceContext::~WakeLockServiceContext() {}

void WakeLockServiceContext::CreateService(
    int render_process_id,
    int render_frame_id,
    mojo::InterfaceRequest<blink::mojom::WakeLockService> request) {
  new WakeLockServiceImpl(weak_factory_.GetWeakPtr(), render_process_id,
                          render_frame_id, std::move(request));
}

void WakeLockServiceContext::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CancelWakeLock(render_frame_host->GetProcess()->GetID(),
                 render_frame_host->GetRoutingID());
}

void WakeLockServiceContext::WebContentsDestroyed() {
#if defined(OS_ANDROID)
  view_weak_factory_.reset();
#endif
}

void WakeLockServiceContext::RequestWakeLock(int render_process_id,
                                             int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RenderFrameHost::FromID(render_process_id, render_frame_id))
    return;

  frames_requesting_lock_.insert(
      std::pair<int, int>(render_process_id, render_frame_id));
  UpdateWakeLock();
}

void WakeLockServiceContext::CancelWakeLock(int render_process_id,
                                            int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frames_requesting_lock_.erase(
      std::pair<int, int>(render_process_id, render_frame_id));
  UpdateWakeLock();
}

bool WakeLockServiceContext::HasWakeLockForTests() const {
  return !!wake_lock_;
}

void WakeLockServiceContext::CreateWakeLock() {
  DCHECK(!wake_lock_);
  wake_lock_.reset(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      device::PowerSaveBlocker::kReasonOther, "Wake Lock API",
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));

#if defined(OS_ANDROID)
  // On Android, additionaly associate the blocker with this WebContents.
  DCHECK(web_contents());

  if (web_contents()->GetNativeView()) {
    view_weak_factory_.reset(new base::WeakPtrFactory<ui::ViewAndroid>(
        web_contents()->GetNativeView()));
    wake_lock_.get()->InitDisplaySleepBlocker(view_weak_factory_->GetWeakPtr());
  }
#endif
}

void WakeLockServiceContext::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
}

void WakeLockServiceContext::UpdateWakeLock() {
  if (!frames_requesting_lock_.empty()) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

}  // namespace content
