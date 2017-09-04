// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

namespace content {

class ScreenOrientationDelegate;
class ScreenOrientationDispatcherHost;
class WebContents;

// Handles screen orientation lock/unlock. Platforms which wish to provide
// custom implementations can provide a factory for ScreenOrientationDelegate.
class CONTENT_EXPORT ScreenOrientationProvider : public WebContentsObserver {
 public:
  ScreenOrientationProvider(ScreenOrientationDispatcherHost* dispatcher_host,
                            WebContents* web_contents);
  ~ScreenOrientationProvider() override;

  // Lock the screen orientation to |orientations|.
  void LockOrientation(int request_id,
                       blink::WebScreenOrientationLockType lock_orientation);

  // Unlock the screen orientation.
  void UnlockOrientation();

  // Inform about a screen orientation update. It is called to let the provider
  // know if a lock has been resolved.
  void OnOrientationChange();

  // Provide a delegate which creates delegates for platform implementations.
  // The delegate is not owned by ScreenOrientationProvider.
  static void SetDelegate(ScreenOrientationDelegate* delegate_);

  // WebContentsObserver
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

 private:
  struct LockInformation {
    LockInformation(int request_id, blink::WebScreenOrientationLockType lock);
    int request_id;
    blink::WebScreenOrientationLockType lock;
  };

  // Returns the lock type that should be associated with 'natural' lock.
  // Returns WebScreenOrientationLockDefault if the natural lock type can't be
  // found.
  blink::WebScreenOrientationLockType GetNaturalLockType() const;

  // Whether the passed |lock| matches the current orientation. In other words,
  // whether the orientation will need to change to match the |lock|.
  bool LockMatchesCurrentOrientation(blink::WebScreenOrientationLockType lock);

  // Not owned, responsible for platform implementations.
  static ScreenOrientationDelegate* delegate_;

  // ScreenOrientationDispatcherHost owns ScreenOrientationProvider.
  ScreenOrientationDispatcherHost* dispatcher_;

  // Whether the ScreenOrientationProvider currently has a lock applied.
  bool lock_applied_;

  // Locks that require orientation changes are not completed until
  // OnOrientationChange.
  std::unique_ptr<LockInformation> pending_lock_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationProvider);
};

} // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_PROVIDER_H_
