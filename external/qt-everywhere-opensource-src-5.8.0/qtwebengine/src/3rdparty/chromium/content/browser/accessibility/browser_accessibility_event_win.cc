// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_event_win.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"

namespace content {

// static
BrowserAccessibilityEvent* BrowserAccessibilityEvent::Create(
    Source source,
    ui::AXEvent event_type,
    const BrowserAccessibility* target) {
  LONG win_event_type = EVENT_MIN;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      win_event_type = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ui::AX_EVENT_ALERT:
      win_event_type = EVENT_SYSTEM_ALERT;
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      win_event_type = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      win_event_type = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_FOCUS:
      win_event_type = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      win_event_type = EVENT_OBJECT_LIVEREGIONCHANGED;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      win_event_type = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      win_event_type = EVENT_SYSTEM_SCROLLINGEND;
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      win_event_type = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      win_event_type = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    default:
      break;
  }

  return new BrowserAccessibilityEventWin(
      source,
      event_type,
      win_event_type,
      target);
}

BrowserAccessibilityEventWin::BrowserAccessibilityEventWin(
    Source source,
    ui::AXEvent event_type,
    LONG win_event_type,
    const BrowserAccessibility* target)
    : BrowserAccessibilityEvent(source, event_type, target),
      win_event_type_(win_event_type) {
}

BrowserAccessibilityEventWin::~BrowserAccessibilityEventWin() {
}

BrowserAccessibilityEvent::Result BrowserAccessibilityEventWin::Fire() {
  DCHECK(target()->manager());

  if (win_event_type_ == EVENT_MIN) {
    delete this;
    return NotNeededOnThisPlatform;
  }

  Result result = target()->manager()->ToBrowserAccessibilityManagerWin()
      ->FireWinAccessibilityEvent(this);

  if (VLOG_IS_ON(1))
    VerboseLog(result);

  delete this;
  return result;
}

std::string BrowserAccessibilityEventWin::GetEventNameStr() {
  std::string result = base::UTF16ToUTF8(AccessibilityEventToString(
      win_event_type_));
  if (event_type() != ui::AX_EVENT_NONE)
    result += "/" + ui::ToString(event_type());
  return result;
}

}  // namespace content
