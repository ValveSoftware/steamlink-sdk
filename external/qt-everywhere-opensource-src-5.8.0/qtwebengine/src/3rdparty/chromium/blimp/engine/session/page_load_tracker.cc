// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/session/page_load_tracker.h"

#include "content/public/browser/render_widget_host_view.h"

namespace blimp {
namespace engine {

namespace {

content::RenderWidgetHost* GetRenderWidgetHostIfMainFrame(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParent() != nullptr)
    return nullptr;

  return render_frame_host->GetView()->GetRenderWidgetHost();
}

}  // namespace

PageLoadTracker::PageLoadTracker(content::WebContents* web_contents,
                                 PageLoadTrackerClient* client)
    : client_(client) {
  DCHECK(web_contents);
  Observe(web_contents);
}

PageLoadTracker::~PageLoadTracker() {}

void PageLoadTracker::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  content::RenderWidgetHost* render_widget_host =
      GetRenderWidgetHostIfMainFrame(render_frame_host);
  if (!render_widget_host)
    return;

  render_widget_load_status_[render_widget_host] = LoadStatus();

  // Notify the client that a navigation was initiated.
  client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADING);
}

void PageLoadTracker::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url) {
  content::RenderWidgetHost* render_widget_host =
      GetRenderWidgetHostIfMainFrame(render_frame_host);
  if (!render_widget_host)
    return;

  RenderWidgetLoadStatusMap::iterator it =
      render_widget_load_status_.find(render_widget_host);
  DCHECK(it != render_widget_load_status_.end());

  it->second.page_loaded = true;
  if (it->second.Loaded()) {
    client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADED);
    render_widget_load_status_.erase(it);
  }
}

void PageLoadTracker::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                  const GURL& validated_url,
                                  int error_code,
                                  const base::string16& error_description,
                                  bool was_ignored_by_handler) {
  content::RenderWidgetHost* render_widget_host =
      GetRenderWidgetHostIfMainFrame(render_frame_host);
  if (!render_widget_host)
    return;

  RenderWidgetLoadStatusMap::iterator it =
      render_widget_load_status_.find(render_widget_host);
  DCHECK(it != render_widget_load_status_.end());

  // If the navigation failed, the client should dismiss the load indicator.
  client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADED);
  render_widget_load_status_.erase(it);
}

void PageLoadTracker::DidFirstPaintAfterLoad(
    content::RenderWidgetHost* render_widget_host) {
  RenderWidgetLoadStatusMap::iterator it =
      render_widget_load_status_.find(render_widget_host);
  DCHECK(it != render_widget_load_status_.end());

  it->second.did_first_paint = true;
  if (it->second.Loaded()) {
    client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADED);
    render_widget_load_status_.erase(it);
  }
}

bool PageLoadTracker::LoadStatus::Loaded() const {
  return page_loaded && did_first_paint;
}

}  // namespace engine
}  // namespace blimp
