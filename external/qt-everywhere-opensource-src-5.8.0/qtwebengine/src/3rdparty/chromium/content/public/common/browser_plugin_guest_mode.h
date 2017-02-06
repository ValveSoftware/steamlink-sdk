// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BROWSER_PLUGIN_GUEST_MODE_H_
#define CONTENT_PUBLIC_COMMON_BROWSER_PLUGIN_GUEST_MODE_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT BrowserPluginGuestMode {
 public:
  // Returns true if inner WebContents should be implemented in terms of cross-
  // process iframes.
  static bool UseCrossProcessFramesForGuests();

 private:
  BrowserPluginGuestMode();  // Not instantiable

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuestMode);
};

}  // namespace

#endif  // CONTENT_PUBLIC_COMMON_BROWSER_PLUGIN_GUEST_MODE_H_
