// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_
#define CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_

#include "content/common/accessibility_mode_enums.h"

struct FrameMsg_Navigate_Type {
 public:
  enum Value {
    // Reload the page, validating cache entries.
    RELOAD,

    // Reload the page, validating only cache entry for the main resource.
    RELOAD_MAIN_RESOURCE,

    // Reload the page, bypassing any cache entries.
    RELOAD_BYPASSING_CACHE,

    // Reload the page using the original request URL.
    RELOAD_ORIGINAL_REQUEST_URL,

    // The navigation is the result of session restore and should honor the
    // page's cache policy while restoring form state. This is set to true if
    // restoring a tab/session from the previous session and the previous
    // session did not crash. If this is not set and the page was restored then
    // the page's cache policy is ignored and we load from the cache.
    RESTORE,

    // Like RESTORE, except that the navigation contains POST data.
    RESTORE_WITH_POST,

    // Navigation type not categorized by the other types.
    NORMAL,

    // Last guard value, so we can use it for validity checks.
    NAVIGATE_TYPE_LAST = NORMAL,
  };
};

struct FrameMsg_UILoadMetricsReportType {
 public:
  enum Value {
    // Do not report metrics for this load.
    NO_REPORT,

    // Report metrics for this load, that originated from clicking on a link.
    REPORT_LINK,

    // Report metrics for this load, that originated from an Android OS intent.
    REPORT_INTENT,

    REPORT_TYPE_LAST = REPORT_INTENT,
  };
};

#endif  // CONTENT_COMMON_FRAME_MESSAGE_ENUMS_H_
