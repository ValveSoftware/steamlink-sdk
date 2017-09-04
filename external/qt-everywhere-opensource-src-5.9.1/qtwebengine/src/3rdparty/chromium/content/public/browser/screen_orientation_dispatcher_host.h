// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationError.h"

namespace content {

// ScreenOrientationDispatcherHost receives lock and unlock requests from the
// RenderFrames and dispatch them to the ScreenOrientationProvider. It also
// make sure that the right RenderFrame get replied for each lock request.
class CONTENT_EXPORT ScreenOrientationDispatcherHost {
 public:
  virtual ~ScreenOrientationDispatcherHost() {}

  // Notifies that the lock with the given |request_id| has succeeded.
  // The renderer process will be notified that the lock succeeded only if
  // |request_id| matches the current lock information.
  virtual void NotifyLockSuccess(int request_id) = 0;

  // Notifies that the lock with the given |request_id| has failed.
  // The renderer process will be notified that the lock succeeded only if
  // |request_id| matches the current lock information.
  virtual void NotifyLockError(int request_id,
                               blink::WebLockOrientationError error) = 0;

  virtual void OnOrientationChange() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
