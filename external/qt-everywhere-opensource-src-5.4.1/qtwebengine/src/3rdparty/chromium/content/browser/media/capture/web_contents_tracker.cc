// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_tracker.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebContentsTracker::WebContentsTracker() {}

WebContentsTracker::~WebContentsTracker() {
  DCHECK(!web_contents()) << "BUG: Still observering!";
}

void WebContentsTracker::Start(int render_process_id, int render_view_id,
                               const ChangeCallback& callback) {
  DCHECK(!message_loop_.get() || message_loop_->BelongsToCurrentThread());

  message_loop_ = base::MessageLoopProxy::current();
  DCHECK(message_loop_.get());
  callback_ = callback;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsTracker::LookUpAndObserveWebContents, this,
                 render_process_id, render_view_id));
}

void WebContentsTracker::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  callback_.Reset();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsTracker::Observe, this,
                 static_cast<WebContents*>(NULL)));
}

void WebContentsTracker::OnWebContentsChangeEvent() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* const wc = web_contents();
  RenderViewHost* const rvh = wc ? wc->GetRenderViewHost() : NULL;
  RenderProcessHost* const rph = rvh ? rvh->GetProcess() : NULL;

  const int render_process_id = rph ? rph->GetID() : MSG_ROUTING_NONE;
  const int render_view_id = rvh ? rvh->GetRoutingID() : MSG_ROUTING_NONE;

  message_loop_->PostTask(FROM_HERE,
      base::Bind(&WebContentsTracker::MaybeDoCallback, this,
                 render_process_id, render_view_id));
}

void WebContentsTracker::MaybeDoCallback(int render_process_id,
                                         int render_view_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!callback_.is_null())
    callback_.Run(render_process_id, render_view_id);
}

void WebContentsTracker::LookUpAndObserveWebContents(int render_process_id,
                                                     int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHost* const rvh =
      RenderViewHost::FromID(render_process_id, render_view_id);
  DVLOG_IF(1, !rvh) << "RenderViewHost::FromID("
                    << render_process_id << ", " << render_view_id
                    << ") returned NULL.";
  Observe(rvh ? WebContents::FromRenderViewHost(rvh) : NULL);
  DVLOG_IF(1, !web_contents())
      << "WebContents::FromRenderViewHost(" << rvh << ") returned NULL.";

  OnWebContentsChangeEvent();
}

void WebContentsTracker::RenderViewReady() {
  OnWebContentsChangeEvent();
}

void WebContentsTracker::AboutToNavigateRenderView(RenderViewHost* rvh) {
  OnWebContentsChangeEvent();
}

void WebContentsTracker::DidNavigateMainFrame(
    const LoadCommittedDetails& details, const FrameNavigateParams& params) {
  OnWebContentsChangeEvent();
}

void WebContentsTracker::WebContentsDestroyed() {
  OnWebContentsChangeEvent();
}

}  // namespace content
