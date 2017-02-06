// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <vector>

#include "base/metrics/histogram.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

namespace {
int g_custom_event_types = ET_LAST;
}  // namespace

std::unique_ptr<Event> EventFromNative(const base::NativeEvent& native_event) {
  std::unique_ptr<Event> event;
  EventType type = EventTypeFromNative(native_event);
  switch(type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      event.reset(new KeyEvent(native_event));
      break;

    case ET_MOUSE_PRESSED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
      event.reset(new MouseEvent(native_event));
      break;

    case ET_MOUSEWHEEL:
      event.reset(new MouseWheelEvent(native_event));
      break;

    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL:
      event.reset(new ScrollEvent(native_event));
      break;

    case ET_TOUCH_RELEASED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_MOVED:
    case ET_TOUCH_CANCELLED:
      event.reset(new TouchEvent(native_event));
      break;

    default:
      break;
  }
  return event;
}

int RegisterCustomEventType() {
  return ++g_custom_event_types;
}

void ValidateEventTimeClock(base::TimeTicks* timestamp) {
// Restrict this validation to DCHECK builds except when using X11 which is
// known to provide bogus timestamps that require correction (crbug.com/611950).
#if defined(USE_X11) || DCHECK_IS_ON()
  if (base::debug::BeingDebugged())
    return;

  base::TimeTicks now = EventTimeForNow();
  int64_t delta = (now - *timestamp).InMilliseconds();
  if (delta < 0 || delta > 60 * 1000) {
    UMA_HISTOGRAM_BOOLEAN("Event.TimestampHasValidTimebase", false);
#if defined(USE_X11)
    *timestamp = now;
#else
    NOTREACHED() << "Unexpected event timestamp, now:" << now
                 << " event timestamp:" << *timestamp;
#endif
  }

  UMA_HISTOGRAM_BOOLEAN("Event.TimestampHasValidTimebase", true);
#endif
}

bool ShouldDefaultToNaturalScroll() {
  return GetInternalDisplayTouchSupport() ==
         display::Display::TOUCH_SUPPORT_AVAILABLE;
}

display::Display::TouchSupport GetInternalDisplayTouchSupport() {
  display::Screen* screen = display::Screen::GetScreen();
  // No screen in some unit tests.
  if (!screen)
    return display::Display::TOUCH_SUPPORT_UNKNOWN;
  const std::vector<display::Display>& displays = screen->GetAllDisplays();
  for (std::vector<display::Display>::const_iterator it = displays.begin();
       it != displays.end(); ++it) {
    if (it->IsInternal())
      return it->touch_support();
  }
  return display::Display::TOUCH_SUPPORT_UNAVAILABLE;
}

void ComputeEventLatencyOS(const base::NativeEvent& native_event) {
  base::TimeTicks current_time = EventTimeForNow();
  base::TimeTicks time_stamp = EventTimeFromNative(native_event);
  base::TimeDelta delta = current_time - time_stamp;

  EventType type = EventTypeFromNative(native_event);
  switch (type) {
    case ET_MOUSEWHEEL:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.MOUSE_WHEEL",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ET_TOUCH_MOVED:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_MOVED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ET_TOUCH_PRESSED:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_PRESSED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    case ET_TOUCH_RELEASED:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.OS.TOUCH_RELEASED",
                                  delta.InMicroseconds(), 1, 1000000, 50);
      return;
    default:
      return;
  }
}

}  // namespace ui
