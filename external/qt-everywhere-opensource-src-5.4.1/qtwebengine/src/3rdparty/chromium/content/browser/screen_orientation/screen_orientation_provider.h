// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_

#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace content {

// Interface that needs to be implemented by any backend that wants to handle
// screen orientation lock/unlock.
class ScreenOrientationProvider {
 public:
  // Lock the screen orientation to |orientations|.
  virtual void LockOrientation(
      blink::WebScreenOrientationLockType orientations) = 0;

  // Unlock the screen orientation.
  virtual void UnlockOrientation() = 0;

  virtual ~ScreenOrientationProvider() {}
};

} // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
