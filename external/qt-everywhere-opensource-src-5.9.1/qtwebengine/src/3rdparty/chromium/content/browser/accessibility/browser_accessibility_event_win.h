// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_WIN_H_

#include <oleacc.h>

#include "content/browser/accessibility/browser_accessibility_event.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityEventWin
    : public BrowserAccessibilityEvent {
 public:
  BrowserAccessibilityEventWin(Source source,
                               ui::AXEvent event_type,
                               LONG win_event_type,
                               const BrowserAccessibility* target);
  ~BrowserAccessibilityEventWin() override;

  LONG win_event_type() { return win_event_type_; }

  Result Fire() override;

 protected:
  std::string GetEventNameStr() override;

  LONG win_event_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityEventWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_EVENT_WIN_H_
