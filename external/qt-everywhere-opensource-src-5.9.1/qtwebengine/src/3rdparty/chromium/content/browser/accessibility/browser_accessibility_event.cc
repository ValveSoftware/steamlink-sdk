// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_event.h"

#include <string>

#include "base/strings/string_util.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

namespace {
std::string ReplaceNewlines(std::string str) {
  std::string result;
  base::ReplaceChars(str, "\n", "\\n", &result);
  return result;
}
}  // namespace

// static
bool BrowserAccessibilityEvent::FailedToSend(Result result) {
  switch (result) {
    case Sent:
    case NotNeededOnThisPlatform:
    case DiscardedBecauseUserNavigatingAway:
    case DiscardedBecauseLiveRegionBusy:
      return false;
    case FailedBecauseNoWindow:
    case FailedBecauseNoFocus:
    case FailedBecauseFrameIsDetached:
      return true;
  }

  NOTREACHED();
  return true;
}

#if !defined(OS_WIN) || defined(TOOLKIT_QT)
// static
BrowserAccessibilityEvent* BrowserAccessibilityEvent::Create(
    Source source,
    ui::AXEvent event_type,
    const BrowserAccessibility* target) {
  return new BrowserAccessibilityEvent(source, event_type, target);
}
#endif  // !defined(OS_WIN)

BrowserAccessibilityEvent::BrowserAccessibilityEvent(
    Source source,
    ui::AXEvent event_type,
    const BrowserAccessibility* target)
    : source_(source),
      event_type_(event_type),
      target_(target),
      original_target_(target) {
  DCHECK(target_);
}

BrowserAccessibilityEvent::~BrowserAccessibilityEvent() {
}

BrowserAccessibilityEvent::Result BrowserAccessibilityEvent::Fire() {
  delete this;
  return NotNeededOnThisPlatform;
}

std::string BrowserAccessibilityEvent::GetEventNameStr() {
  return ui::ToString(event_type_);
}

void BrowserAccessibilityEvent::VerboseLog(Result result) {
  std::string event_name = GetEventNameStr();

  const char* result_str = nullptr;
  switch (result) {
    case Sent:
      result_str = "Sent";
      break;
    case NotNeededOnThisPlatform:
      result_str = "NotNeededOnThisPlatform";
      break;
    case DiscardedBecauseUserNavigatingAway:
      result_str = "DiscardedBecauseUserNavigatingAway";
      break;
    case DiscardedBecauseLiveRegionBusy:
      result_str = "DiscardedBecauseLiveRegionBusy";
      break;
    case FailedBecauseNoWindow:
      result_str = "FailedBecauseNoWindow";
      break;
    case FailedBecauseNoFocus:
      result_str = "FailedBecauseNoFocus";
      break;
    case FailedBecauseFrameIsDetached:
      result_str = "FailedBecauseFrameIsDetached";
      break;
  }

  const char* success_str = (result == Sent ? "+" : "-");

  const char* source_str = nullptr;
  switch (source_) {
    case FromBlink:
      source_str = "FromBlink";
      break;
    case FromChildFrameLoading:
      source_str = "FromChildFrameLoading";
      break;
    case FromFindInPageResult:
      source_str = "FromFindInPageResult";
      break;
    case FromRenderFrameHost:
      source_str = "FromRenderFrameHost";
      break;
    case FromScroll:
      source_str = "FromScroll";
      break;
    case FromTreeChange:
      source_str = "FromTreeChange";
      break;
    case FromWindowFocusChange:
      source_str = "FromWindowFocusChange";
      break;
    case FromPendingLoadComplete:
      source_str = "FromPendingLoadComplete";
      break;
  }

  std::string original_target_str;
  if (original_target_ != target_) {
    original_target_str = " originalTarget=[["
        + ReplaceNewlines(original_target_->GetData().ToString()) + "]]";
  }

  VLOG(1) << "Accessibility event"
          << " " << success_str
          << " " << event_name
          << " result=" << result_str
          << " source=" << source_str
          << " unique_id=" << target_->unique_id()
          << " target=[["
          << ReplaceNewlines(target_->GetData().ToString()) << "]]"
          << original_target_str;
}

}  // namespace content
