// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_DEBUG_URLS_H_
#define CONTENT_BROWSER_FRAME_HOST_DEBUG_URLS_H_

#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

// Checks if the given url is a url used for debugging purposes, and if so
// handles it and returns true.
bool HandleDebugURL(const GURL& url, ui::PageTransition transition);

// Returns whether the given url is either a debugging url handled in the
// renderer process, such as one that crashes or hangs the renderer, or a
// javascript: URL that operates on the current page in the renderer.  Such URLs
// do not represent actual navigations and can be loaded in any SiteInstance.
bool IsRendererDebugURL(const GURL& url);

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_DEBUG_URLS_H_
