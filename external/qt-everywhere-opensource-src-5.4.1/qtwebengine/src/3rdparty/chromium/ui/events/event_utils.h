// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_UTILS_H_
#define UI_EVENTS_EVENT_UTILS_H_

#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/events/events_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace gfx {
class Point;
class Vector2d;
}

namespace base {
class TimeDelta;
}

namespace ui {

class Event;

// Updates the list of devices for cached properties.
EVENTS_EXPORT void UpdateDeviceList();

// Returns a ui::Event wrapping a native event. Ownership of the returned value
// is transferred to the caller.
EVENTS_EXPORT scoped_ptr<Event> EventFromNative(
    const base::NativeEvent& native_event);

// Get the EventType from a native event.
EVENTS_EXPORT EventType EventTypeFromNative(
    const base::NativeEvent& native_event);

// Get the EventFlags from a native event.
EVENTS_EXPORT int EventFlagsFromNative(const base::NativeEvent& native_event);

// Get the timestamp from a native event.
EVENTS_EXPORT base::TimeDelta EventTimeFromNative(
    const base::NativeEvent& native_event);

// Create a timestamp based on the current time.
EVENTS_EXPORT base::TimeDelta EventTimeForNow();

// Get the location from a native event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
// TODO(tdresser): Return gfx::PointF here. See crbug.com/337827.
EVENTS_EXPORT gfx::Point EventLocationFromNative(
    const base::NativeEvent& native_event);

// Gets the location in native system coordinate space.
EVENTS_EXPORT gfx::Point EventSystemLocationFromNative(
    const base::NativeEvent& native_event);

#if defined(USE_X11)
// Returns the 'real' button for an event. The button reported in slave events
// does not take into account any remapping (e.g. using xmodmap), while the
// button reported in master events do. This is a utility function to always
// return the mapped button.
EVENTS_EXPORT int EventButtonFromNative(const base::NativeEvent& native_event);
#endif

// Returns the KeyboardCode from a native event.
EVENTS_EXPORT KeyboardCode KeyboardCodeFromNative(
    const base::NativeEvent& native_event);

// Returns the DOM KeyboardEvent code (physical location in the
// keyboard) from a native event.  The ownership of the return value
// is NOT trasferred to the caller.
EVENTS_EXPORT const char* CodeFromNative(
    const base::NativeEvent& native_event);

// Returns the platform related key code. For X11, it is xksym value.
EVENTS_EXPORT uint32 PlatformKeycodeFromNative(
    const base::NativeEvent& native_event);

// Returns the flags of the button that changed during a press/release.
EVENTS_EXPORT int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event);

// Gets the mouse wheel offsets from a native event.
EVENTS_EXPORT gfx::Vector2d GetMouseWheelOffset(
    const base::NativeEvent& native_event);

// Returns a copy of |native_event|. Depending on the platform, this copy may
// need to be deleted with ReleaseCopiedNativeEvent().
base::NativeEvent CopyNativeEvent(
    const base::NativeEvent& native_event);

// Delete a |native_event| previously created by CopyNativeEvent().
void ReleaseCopiedNativeEvent(
    const base::NativeEvent& native_event);

// Gets the touch id from a native event.
EVENTS_EXPORT int GetTouchId(const base::NativeEvent& native_event);

// Clear the touch id from bookkeeping if it is a release/cancel event.
EVENTS_EXPORT void ClearTouchIdIfReleased(
    const base::NativeEvent& native_event);

// Gets the radius along the X/Y axis from a native event. Default is 1.0.
EVENTS_EXPORT float GetTouchRadiusX(const base::NativeEvent& native_event);
EVENTS_EXPORT float GetTouchRadiusY(const base::NativeEvent& native_event);

// Gets the angle of the major axis away from the X axis. Default is 0.0.
EVENTS_EXPORT float GetTouchAngle(const base::NativeEvent& native_event);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
EVENTS_EXPORT float GetTouchForce(const base::NativeEvent& native_event);

// Gets the fling velocity from a native event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
EVENTS_EXPORT bool GetFlingData(const base::NativeEvent& native_event,
                            float* vx,
                            float* vy,
                            float* vx_ordinal,
                            float* vy_ordinal,
                            bool* is_cancel);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset|, |y_offset| and |finger_count| can be NULL.
EVENTS_EXPORT bool GetScrollOffsets(const base::NativeEvent& native_event,
                                float* x_offset,
                                float* y_offset,
                                float* x_offset_ordinal,
                                float* y_offset_ordinal,
                                int* finger_count);

EVENTS_EXPORT bool GetGestureTimes(const base::NativeEvent& native_event,
                               double* start_time,
                               double* end_time);

// Returns whether natural scrolling should be used for touchpad.
EVENTS_EXPORT bool ShouldDefaultToNaturalScroll();

// Returns whether or not the internal display produces touch events.
EVENTS_EXPORT gfx::Display::TouchSupport GetInternalDisplayTouchSupport();

// Was this event generated by a touchpad device?
// The caller is responsible for ensuring that this is a mouse/touchpad event
// before calling this function.
EVENTS_EXPORT bool IsTouchpadEvent(const base::NativeEvent& event);

#if defined(OS_WIN)
EVENTS_EXPORT int GetModifiersFromACCEL(const ACCEL& accel);
EVENTS_EXPORT int GetModifiersFromKeyState();

// Returns true if |message| identifies a mouse event that was generated as the
// result of a touch event.
EVENTS_EXPORT bool IsMouseEventFromTouch(UINT message);

// Converts scan code and lParam each other.  The scan code
// representing an extended key contains 0xE000 bits.
EVENTS_EXPORT uint16 GetScanCodeFromLParam(LPARAM lParam);
EVENTS_EXPORT LPARAM GetLParamFromScanCode(uint16 scan_code);

#endif

// Registers a custom event type.
EVENTS_EXPORT int RegisterCustomEventType();

}  // namespace ui

#endif  // UI_EVENTS_EVENT_UTILS_H_
