// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"

namespace ui {

class Event;
class EventDeviceInfo;
class EventModifiersEvdev;
class CursorDelegateEvdev;

typedef base::Callback<void(Event*)> EventDispatchCallback;

// Convert libevdev-cros events to ui::Events using libgestures.
//
// This builds a GestureInterpreter for an input device (touchpad or
// mouse).
//
// Raw input events must be preprocessed into a form suitable for
// libgestures. The kernel protocol only emits changes to the device state,
// so changes must be accumulated until a sync event. The full device state
// at sync is then processed by libgestures.
//
// Once we have the state at sync, we convert it to a HardwareState object
// and forward it to libgestures. If any gestures are produced, they are
// converted to ui::Events and dispatched.
class EVENTS_OZONE_EVDEV_EXPORT GestureInterpreterLibevdevCros
    : public EventReaderLibevdevCros::Delegate {
 public:
  GestureInterpreterLibevdevCros(EventModifiersEvdev* modifiers,
                                 CursorDelegateEvdev* cursor,
                                 const EventDispatchCallback& callback);
  virtual ~GestureInterpreterLibevdevCros();

  // Overriden from ui::EventReaderLibevdevCros::Delegate
  virtual void OnLibEvdevCrosOpen(Evdev* evdev,
                                  EventStateRec* evstate) OVERRIDE;
  virtual void OnLibEvdevCrosEvent(Evdev* evdev,
                                   EventStateRec* evstate,
                                   const timeval& time) OVERRIDE;

  // Handler for gesture events generated from libgestures.
  void OnGestureReady(const Gesture* gesture);

 private:
  void OnGestureMove(const Gesture* gesture, const GestureMove* move);
  void OnGestureScroll(const Gesture* gesture, const GestureScroll* move);
  void OnGestureButtonsChange(const Gesture* gesture,
                              const GestureButtonsChange* move);
  void Dispatch(Event* event);
  void DispatchMouseButton(unsigned int modifier, bool down);

  // Shared modifier state.
  EventModifiersEvdev* modifiers_;

  // Shared cursor state.
  CursorDelegateEvdev* cursor_;

  // Callback for dispatching events.
  EventDispatchCallback dispatch_callback_;

  // Gestures interpretation state.
  gestures::GestureInterpreter* interpreter_;

  DISALLOW_COPY_AND_ASSIGN(GestureInterpreterLibevdevCros);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_
