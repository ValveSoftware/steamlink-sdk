// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PageNavigator defines an interface that can be used to express the user's
// intention to navigate to a particular URL.  The implementing class should
// perform the navigation.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {

class WebContents;

struct CONTENT_EXPORT OpenURLParams {
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                WindowOpenDisposition disposition,
                ui::PageTransition transition,
                bool is_renderer_initiated);
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                int frame_tree_node_id,
                WindowOpenDisposition disposition,
                ui::PageTransition transition,
                bool is_renderer_initiated);
  OpenURLParams(const OpenURLParams& other);
  ~OpenURLParams();

  // The URL/referrer to be opened.
  GURL url;
  Referrer referrer;

  // SiteInstance of the frame that initiated the navigation or null if we
  // don't know it.
  scoped_refptr<content::SiteInstance> source_site_instance;

  // Any redirect URLs that occurred for this navigation before |url|.
  std::vector<GURL> redirect_chain;

  // Indicates whether this navigation will be sent using POST.
  bool uses_post;

  // The post data when the navigation uses POST.
  scoped_refptr<ResourceRequestBody> post_data;

  // Extra headers to add to the request for this page.  Headers are
  // represented as "<name>: <value>" and separated by \r\n.  The entire string
  // is terminated by \r\n.  May be empty if no extra headers are needed.
  std::string extra_headers;

  // The browser-global FrameTreeNode ID or -1 to indicate the main frame.
  int frame_tree_node_id;

  // The disposition requested by the navigation source.
  WindowOpenDisposition disposition;

  // The transition type of navigation.
  ui::PageTransition transition;

  // Whether this navigation is initiated by the renderer process.
  bool is_renderer_initiated;

  // Indicates whether this navigation should replace the current
  // navigation entry.
  bool should_replace_current_entry;

  // Indicates whether this navigation was triggered while processing a user
  // gesture if the navigation was initiated by the renderer.
  bool user_gesture;

 private:
  OpenURLParams();
};

class PageNavigator {
 public:
  virtual ~PageNavigator() {}

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  // Returns the WebContents the URL is opened in, or nullptr if the URL wasn't
  // opened immediately.
  virtual WebContents* OpenURL(const OpenURLParams& params) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
