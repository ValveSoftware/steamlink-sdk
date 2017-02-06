// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_SESSION_TAB_H_
#define BLIMP_ENGINE_SESSION_TAB_H_

#include "base/macros.h"
#include "blimp/engine/session/page_load_tracker.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderViewHost;
class WebContents;
}

namespace blimp {

class BlimpMessageProcessor;

namespace engine {

class EngineRenderWidgetFeature;

// Owns WebContents, handles operations such as navigation requests in the tab,
// and has one-to-one mapping to a client tab.
class Tab : public content::WebContentsObserver, public PageLoadTrackerClient {
 public:
  // Caller ensures |render_widget_feature| and |navigation_message_sender|
  // outlives this object.
  // |web_contents|: the WebContents this tab owns.
  // |tab_id|: the ID of this tab.
  // |render_widget_feature|: render widget feature.
  // |navigation_message_sender|: for sending navigation messages to client.
  Tab(std::unique_ptr<content::WebContents> web_contents,
      const int tab_id,
      EngineRenderWidgetFeature* render_widget_feature,
      BlimpMessageProcessor* navigation_message_sender);
  ~Tab() override;

  content::WebContents* web_contents() { return web_contents_.get(); }
  int tab_id() const { return tab_id_; }

  // Resizes the tab to |size_in_dips| with |device_pixel_ratio|.
  void Resize(float device_pixel_ratio, const gfx::Size& size_in_dips);

  // Handles tab navigation.
  void LoadUrl(const GURL& url);
  void GoBack();
  void GoForward();
  void Reload();

  // Handles navigation state changed event.
  void NavigationStateChanged(content::InvalidateTypes changed_flags);

  // PageLoadTrackerClient implementation.
  void SendPageLoadStatusUpdate(PageLoadStatus load_status) override;

 private:
  // content::WebContentsObserver implementation.
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;

  std::unique_ptr<content::WebContents> web_contents_;
  const int tab_id_;
  EngineRenderWidgetFeature* render_widget_feature_;
  BlimpMessageProcessor* navigation_message_sender_;

  // Tracks the page load status for a tab.
  PageLoadTracker page_load_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_SESSION_TAB_H_
