// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_MAC_H_

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/capture/window_activity_tracker.h"
#include "content/common/content_export.h"
#include "ui/base/cocoa/tracking_area.h"
#include "ui/gfx/native_widget_types.h"

@interface MouseTracker : NSObject {
 @private
  ui::ScopedCrTrackingArea trackingArea_;

  // The view on which mouse movement is detected.
  NSView* nsView_;

  // Runs on any mouse interaction from user.
  base::Closure mouseInteractionObserver_;
}

- (instancetype)initWithView:(NSView*)nsView;

// Register an observer for mouse interaction.
- (void)registerMouseInteractionObserver:(const base::Closure&)observer;

@end

namespace content {

// Tracks UI events and makes a decision on whether the user has been
// actively interacting with a specified window.
class CONTENT_EXPORT WindowActivityTrackerMac : public WindowActivityTracker {
 public:
  explicit WindowActivityTrackerMac(NSView* view);
  ~WindowActivityTrackerMac() final;

  base::WeakPtr<WindowActivityTracker> GetWeakPtr() final;

 private:
  void OnMouseActivity();

  base::scoped_nsobject<MouseTracker> mouse_tracker_;

  base::WeakPtrFactory<WindowActivityTrackerMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowActivityTrackerMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WINDOW_ACTIVITY_TRACKER_MAC_H_
