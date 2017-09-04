// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_

namespace content {

// Used to specify detailed behavior on requesting reloads. NONE is used in
// general, but behaviors depend on context. If NONE is used for tab restore, or
// history navigation, it loads preferring cache (which may be stale).
enum class ReloadType {
  NONE,                  // Normal load, restore, or history navigation.
  NORMAL,                // Normal (cache-validating) reload.
  MAIN_RESOURCE,         // Reload validating only the main resource.
  BYPASSING_CACHE,       // Reload bypassing the cache (shift-reload).
  ORIGINAL_REQUEST_URL,  // Reload using the original request URL.
  DISABLE_LOFI_MODE      // Reload with Lo-Fi mode disabled.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RELOAD_TYPE_H_
