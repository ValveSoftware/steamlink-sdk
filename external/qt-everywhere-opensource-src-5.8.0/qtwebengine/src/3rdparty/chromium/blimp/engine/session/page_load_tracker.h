// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_
#define BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace blimp {
namespace engine {

enum class PageLoadStatus { LOADING, LOADED };

class PageLoadTrackerClient {
 public:
  virtual ~PageLoadTrackerClient() {}

  // Called when a navigation status update should be sent to the client.
  // |load_status| indicates the status of the navigation. An update with a
  // |LOADING| value is sent each time a new navigation is initiated. This
  // update will be followed by a |LOADED| value when the navigation either
  // successfully finishes or fails.
  virtual void SendPageLoadStatusUpdate(PageLoadStatus load_status) = 0;
};

// Tracks the page load status for a tab using load and paint notifications
// from the renderer.
class PageLoadTracker : public content::WebContentsObserver {
 public:
  explicit PageLoadTracker(content::WebContents* web_contents,
                           PageLoadTrackerClient* client);
  ~PageLoadTracker() override;

 private:
  struct LoadStatus {
    // Set to true on receiving a notification from the renderer that the load
    // finished. See WebContentsObserver::DidFinishLoad.
    bool page_loaded = false;

    // Set to true on receiving a notification from the renderer that the first
    // paint after a navigation was performed.
    // See WebContentsObserver::DidFirstPaintAfterLoad.
    bool did_first_paint = false;

    bool Loaded() const;
  };

  typedef base::SmallMap<std::map<content::RenderWidgetHost*, LoadStatus>>
      RenderWidgetLoadStatusMap;

  // content::WebContentsObserver implementation.
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override;
  void DidFirstPaintAfterLoad(
      content::RenderWidgetHost* render_widget_host) override;

  RenderWidgetLoadStatusMap render_widget_load_status_;

  PageLoadTrackerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTracker);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_
