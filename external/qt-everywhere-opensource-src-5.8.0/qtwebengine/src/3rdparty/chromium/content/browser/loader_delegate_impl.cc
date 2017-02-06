// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader_delegate_impl.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void NotifyLoadStateChangedOnUI(int child_id,
                                int route_id,
                                const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64_t upload_position,
                                uint64_t upload_size) {
  RenderViewHostImpl* view = RenderViewHostImpl::FromID(child_id, route_id);
  if (view)
    view->LoadStateChanged(url, load_state, upload_position, upload_size);
}

void DidGetResourceResponseStartOnUI(
    int render_process_id,
    int render_frame_host,
    std::unique_ptr<ResourceRequestDetails> details) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host));
  if (!web_contents)
    return;
  web_contents->DidGetResourceResponseStart(*details.get());
}

void DidGetRedirectForResourceRequestOnUI(
    int render_process_id,
    int render_frame_host,
    std::unique_ptr<ResourceRedirectDetails> details) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_host);
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host));
  if (!web_contents)
    return;
  web_contents->DidGetRedirectForResourceRequest(host, *details.get());
}

}  // namespace

LoaderDelegateImpl::~LoaderDelegateImpl() {}

void LoaderDelegateImpl::LoadStateChanged(
    int child_id,
    int route_id,
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyLoadStateChangedOnUI, child_id, route_id, url,
                 load_state, upload_position, upload_size));
}

void LoaderDelegateImpl::DidGetResourceResponseStart(
    int render_process_id,
    int render_frame_host,
    std::unique_ptr<ResourceRequestDetails> details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DidGetResourceResponseStartOnUI, render_process_id,
                 render_frame_host, base::Passed(std::move(details))));
}

void LoaderDelegateImpl::DidGetRedirectForResourceRequest(
    int render_process_id,
    int render_frame_host,
    std::unique_ptr<ResourceRedirectDetails> details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DidGetRedirectForResourceRequestOnUI, render_process_id,
                 render_frame_host, base::Passed(std::move(details))));
}

}  // namespace content
