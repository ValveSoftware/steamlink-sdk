// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_url_parameters.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {

DownloadUrlParameters::DownloadUrlParameters(
    const GURL& url,
    int render_process_host_id,
    int render_view_host_routing_id,
    ResourceContext* resource_context)
    : content_initiated_(false),
      load_flags_(0),
      method_("GET"),
      post_id_(-1),
      prefer_cache_(false),
      render_process_host_id_(render_process_host_id),
      render_view_host_routing_id_(render_view_host_routing_id),
      resource_context_(resource_context),
      url_(url) {
}

DownloadUrlParameters::~DownloadUrlParameters() {
}

// static
DownloadUrlParameters* DownloadUrlParameters::FromWebContents(
    WebContents* web_contents,
    const GURL& url) {
  return new DownloadUrlParameters(
      url,
      web_contents->GetRenderProcessHost()->GetID(),
      web_contents->GetRenderViewHost()->GetRoutingID(),
      web_contents->GetBrowserContext()->GetResourceContext());
}

}  // namespace content
