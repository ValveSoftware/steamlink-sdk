// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_H_

#include <string>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_enums.h"

namespace content {

class BrowserAccessibility;

class CONTENT_EXPORT BrowserAccessibilityEvent {
 public:
  enum Source {
    FromBlink,
    FromChildFrameLoading,
    FromFindInPageResult,
    FromRenderFrameHost,
    FromScroll,
    FromTreeChange,
    FromWindowFocusChange,
    FromPendingLoadComplete,
  };

  enum Result {
    Sent,
    NotNeededOnThisPlatform,
    DiscardedBecauseUserNavigatingAway,
    DiscardedBecauseLiveRegionBusy,
    FailedBecauseNoWindow,
    FailedBecauseNoFocus,
    FailedBecauseFrameIsDetached,
  };

  // Returns true if the given result indicates that sending the event failed.
  // Returns false if the event was sent, or if it was not needed (which is
  // not considered failure).
  static bool FailedToSend(Result result);

  static BrowserAccessibilityEvent* Create(Source source,
                                           ui::AXEvent event_type,
                                           const BrowserAccessibility* target);

  // Construct a new accessibility event that should be sent via a
  // platform-specific notification.
  BrowserAccessibilityEvent(Source source,
                            ui::AXEvent event_type,
                            const BrowserAccessibility* target);
  virtual ~BrowserAccessibilityEvent();

  // Synchronously try to send the native accessibility event notification
  // and return a result indicating the outcome. This deletes the event.
  virtual Result Fire();

  Source source() { return source_; }
  ui::AXEvent event_type() { return event_type_; }
  const BrowserAccessibility* target() { return target_; }
  void set_target(const BrowserAccessibility* target) { target_ = target; }
  void set_original_target(BrowserAccessibility* target) {
    original_target_ = target;
  }

 protected:
  virtual std::string GetEventNameStr();
  void VerboseLog(Result result);

 private:
  Source source_;
  ui::AXEvent event_type_;
  const BrowserAccessibility* target_;
  const BrowserAccessibility* original_target_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityEvent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_H_
