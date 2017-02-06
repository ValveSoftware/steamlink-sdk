// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_url_parameters.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {

DownloadUrlParameters::DownloadUrlParameters(
    const GURL& url,
    net::URLRequestContextGetter* url_request_context_getter)
    : DownloadUrlParameters(url, -1, -1, -1, url_request_context_getter) {
}

DownloadUrlParameters::DownloadUrlParameters(
    const GURL& url,
    int render_process_host_id,
    int render_view_host_routing_id,
    int render_frame_host_routing_id,
    net::URLRequestContextGetter* url_request_context_getter)
    : content_initiated_(false),
      method_("GET"),
      post_id_(-1),
      prefer_cache_(false),
      render_process_host_id_(render_process_host_id),
      render_view_host_routing_id_(render_view_host_routing_id),
      render_frame_host_routing_id_(render_frame_host_routing_id),
      url_request_context_getter_(url_request_context_getter),
      url_(url),
      do_not_prompt_for_login_(false) {}

DownloadUrlParameters::~DownloadUrlParameters() {
}

// static
std::unique_ptr<DownloadUrlParameters>
DownloadUrlParameters::CreateForWebContentsMainFrame(
    WebContents* web_contents,
    const GURL& url) {
  RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      web_contents->GetBrowserContext(), render_frame_host->GetSiteInstance());
  return std::unique_ptr<DownloadUrlParameters>(new DownloadUrlParameters(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRenderViewHost()->GetRoutingID(),
      render_frame_host->GetRoutingID(),
      storage_partition->GetURLRequestContext()));
}

}  // namespace content
