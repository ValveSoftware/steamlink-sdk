// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_tracker.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebContentsTracker::WebContentsTracker(bool track_fullscreen_rwh)
    : track_fullscreen_rwh_(track_fullscreen_rwh),
      last_target_(NULL) {}

WebContentsTracker::~WebContentsTracker() {
  DCHECK(!web_contents()) << "BUG: Still observering!";
}

void WebContentsTracker::Start(int render_process_id, int main_render_frame_id,
                               const ChangeCallback& callback) {
  DCHECK(!task_runner_ || task_runner_->BelongsToCurrentThread());

  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner_);
  callback_ = callback;

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    StartObservingWebContents(render_process_id, main_render_frame_id);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebContentsTracker::StartObservingWebContents, this,
                   render_process_id, main_render_frame_id));
  }
}

void WebContentsTracker::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  callback_.Reset();
  resize_callback_.Reset();

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    WebContentsObserver::Observe(NULL);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebContentsTracker::Observe, this,
                   static_cast<WebContents*>(NULL)));
  }
}

RenderWidgetHost* WebContentsTracker::GetTargetRenderWidgetHost() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const wc = web_contents();
  if (!wc)
    return NULL;

  RenderWidgetHost* rwh = NULL;
  if (track_fullscreen_rwh_) {
    RenderWidgetHostView* const view = wc->GetFullscreenRenderWidgetHostView();
    if (view)
      rwh = view->GetRenderWidgetHost();
  }
  if (!rwh) {
    RenderFrameHostImpl* const rfh =
        static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
    if (rfh)
      rwh = rfh->GetRenderWidgetHost();
  }

  return rwh;
}

void WebContentsTracker::SetResizeChangeCallback(
    const base::Closure& callback) {
  DCHECK(!task_runner_ || task_runner_->BelongsToCurrentThread());
  resize_callback_ = callback;
}

void WebContentsTracker::OnPossibleTargetChange(bool force_callback_run) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHost* const rwh = GetTargetRenderWidgetHost();
  if (rwh == last_target_ && !force_callback_run)
    return;
  DVLOG(1) << "Will report target change from RenderWidgetHost@" << last_target_
           << " to RenderWidgetHost@" << rwh;
  last_target_ = rwh;

  if (task_runner_->BelongsToCurrentThread()) {
    MaybeDoCallback(rwh != nullptr);
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebContentsTracker::MaybeDoCallback, this, rwh != nullptr));
}

void WebContentsTracker::MaybeDoCallback(bool was_still_tracking) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!callback_.is_null())
    callback_.Run(was_still_tracking);
  if (was_still_tracking)
    MaybeDoResizeCallback();
}

void WebContentsTracker::MaybeDoResizeCallback() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!resize_callback_.is_null())
    resize_callback_.Run();
}

void WebContentsTracker::StartObservingWebContents(int render_process_id,
                                                   int main_render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Observe(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(
      render_process_id, main_render_frame_id)));
  DVLOG_IF(1, !web_contents())
      << "Could not find WebContents associated with main RenderFrameHost "
      << "referenced by render_process_id=" << render_process_id
      << ", routing_id=" << main_render_frame_id;

  OnPossibleTargetChange(true);
}

void WebContentsTracker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  OnPossibleTargetChange(false);
}

void WebContentsTracker::RenderFrameHostChanged(RenderFrameHost* old_host,
                                                RenderFrameHost* new_host) {
  OnPossibleTargetChange(false);
}

void WebContentsTracker::MainFrameWasResized(bool width_changed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (task_runner_->BelongsToCurrentThread()) {
    MaybeDoResizeCallback();
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebContentsTracker::MaybeDoResizeCallback, this));
}

void WebContentsTracker::WebContentsDestroyed() {
  Observe(NULL);
  OnPossibleTargetChange(false);
}

void WebContentsTracker::DidShowFullscreenWidget() {
  OnPossibleTargetChange(false);
}

void WebContentsTracker::DidDestroyFullscreenWidget() {
  OnPossibleTargetChange(false);
}

}  // namespace content
