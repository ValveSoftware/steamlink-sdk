// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_

namespace content {

// Flags passed to the WebContentsDelegate.NavigationStateChanged to tell it
// what has changed. Combine them to update more than one thing.
enum InvalidateTypes {
  INVALIDATE_TYPE_URL           = 1 << 0,  // The URL has changed.
  INVALIDATE_TYPE_TAB           = 1 << 1,  // The favicon, app icon, or crashed
                                           // state changed.
  INVALIDATE_TYPE_LOAD          = 1 << 2,  // The loading state has changed.
  INVALIDATE_TYPE_PAGE_ACTIONS  = 1 << 3,  // Page action icons have changed.
  INVALIDATE_TYPE_TITLE         = 1 << 4,  // The title changed.
};

}

#endif  // CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
