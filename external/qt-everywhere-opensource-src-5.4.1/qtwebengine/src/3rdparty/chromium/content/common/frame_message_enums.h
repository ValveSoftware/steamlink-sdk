// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGES_ENUMS_H_
#define CONTENT_COMMON_FRAME_MESSAGES_ENUMS_H_

#include "ipc/ipc_message_macros.h"

struct FrameMsg_Navigate_Type {
 public:
  enum Value {
    // Reload the page.
    RELOAD,

    // Reload the page, ignoring any cache entries.
    RELOAD_IGNORING_CACHE,

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

#endif  // CONTENT_COMMON_FRAME_MESSAGES_ENUMS_H_
