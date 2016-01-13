// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>

#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_timer_provider.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace {

// Convert libevdev device class to libgestures device class.
GestureInterpreterDeviceClass GestureDeviceClass(Evdev* evdev) {
  switch (evdev->info.evdev_class) {
    case EvdevClassMouse:
      return GESTURES_DEVCLASS_MOUSE;
    case EvdevClassMultitouchMouse:
      return GESTURES_DEVCLASS_MULTITOUCH_MOUSE;
    case EvdevClassTouchpad:
      return GESTURES_DEVCLASS_TOUCHPAD;
    case EvdevClassTouchscreen:
      return GESTURES_DEVCLASS_TOUCHSCREEN;
    default:
      return GESTURES_DEVCLASS_UNKNOWN;
  }
}

// Convert libevdev state to libgestures hardware properties.
HardwareProperties GestureHardwareProperties(Evdev* evdev) {
  HardwareProperties hwprops;
  hwprops.left = Event_Get_Left(evdev);
  hwprops.top = Event_Get_Top(evdev);
  hwprops.right = Event_Get_Right(evdev);
  hwprops.bottom = Event_Get_Bottom(evdev);
  hwprops.res_x = Event_Get_Res_X(evdev);
  hwprops.res_y = Event_Get_Res_Y(evdev);
  hwprops.screen_x_dpi = 133;
  hwprops.screen_y_dpi = 133;
  hwprops.orientation_minimum = Event_Get_Orientation_Minimum(evdev);
  hwprops.orientation_maximum = Event_Get_Orientation_Maximum(evdev);
  hwprops.max_finger_cnt = Event_Get_Slot_Count(evdev);
  hwprops.max_touch_cnt = Event_Get_Touch_Count_Max(evdev);
  hwprops.supports_t5r2 = Event_Get_T5R2(evdev);
  hwprops.support_semi_mt = Event_Get_Semi_MT(evdev);
  /* buttonpad means a physical button under the touch surface */
  hwprops.is_button_pad = Event_Get_Button_Pad(evdev);
  return hwprops;
}

// Callback from libgestures when a gesture is ready.
void OnGestureReadyHelper(void* client_data, const Gesture* gesture) {
  GestureInterpreterLibevdevCros* interpreter =
      static_cast<GestureInterpreterLibevdevCros*>(client_data);
  interpreter->OnGestureReady(gesture);
}

// Convert gestures timestamp (stime_t) to ui::Event timestamp.
base::TimeDelta StimeToTimedelta(stime_t timestamp) {
  return base::TimeDelta::FromMicroseconds(timestamp *
                                           base::Time::kMicrosecondsPerSecond);
}

// Number of fingers for scroll gestures.
const int kGestureScrollFingerCount = 2;

}  // namespace

GestureInterpreterLibevdevCros::GestureInterpreterLibevdevCros(
    EventModifiersEvdev* modifiers,
    CursorDelegateEvdev* cursor,
    const EventDispatchCallback& callback)
    : modifiers_(modifiers),
      cursor_(cursor),
      dispatch_callback_(callback),
      interpreter_(NULL) {}

GestureInterpreterLibevdevCros::~GestureInterpreterLibevdevCros() {
  if (interpreter_) {
    DeleteGestureInterpreter(interpreter_);
    interpreter_ = NULL;
  }
}

void GestureInterpreterLibevdevCros::OnLibEvdevCrosOpen(
    Evdev* evdev,
    EventStateRec* evstate) {
  CHECK(evdev->info.is_monotonic) << "libevdev must use monotonic timestamps";
  VLOG(9) << "HACK DO NOT REMOVE OR LINK WILL FAIL" << (void*)gestures_log;

  HardwareProperties hwprops = GestureHardwareProperties(evdev);
  GestureInterpreterDeviceClass devclass = GestureDeviceClass(evdev);

  // Create & initialize GestureInterpreter.
  CHECK(!interpreter_);
  interpreter_ = NewGestureInterpreter();
  GestureInterpreterInitialize(interpreter_, devclass);
  GestureInterpreterSetHardwareProperties(interpreter_, &hwprops);
  GestureInterpreterSetTimerProvider(
      interpreter_,
      const_cast<GesturesTimerProvider*>(&kGestureTimerProvider),
      this);
  GestureInterpreterSetCallback(interpreter_, OnGestureReadyHelper, this);
}

void GestureInterpreterLibevdevCros::OnLibEvdevCrosEvent(Evdev* evdev,
                                                         EventStateRec* evstate,
                                                         const timeval& time) {
  HardwareState hwstate;
  memset(&hwstate, 0, sizeof(hwstate));
  hwstate.timestamp = StimeFromTimeval(&time);

  // Mouse.
  hwstate.rel_x = evstate->rel_x;
  hwstate.rel_y = evstate->rel_y;
  hwstate.rel_wheel = evstate->rel_wheel;
  hwstate.rel_hwheel = evstate->rel_hwheel;

  // Touch.
  FingerState fingers[Event_Get_Slot_Count(evdev)];
  memset(&fingers, 0, sizeof(fingers));
  int current_finger = 0;
  for (int i = 0; i < evstate->slot_count; i++) {
    MtSlotPtr slot = &evstate->slots[i];
    if (slot->tracking_id == -1)
      continue;
    fingers[current_finger].touch_major = slot->touch_major;
    fingers[current_finger].touch_minor = slot->touch_minor;
    fingers[current_finger].width_major = slot->width_major;
    fingers[current_finger].width_minor = slot->width_minor;
    fingers[current_finger].pressure = slot->pressure;
    fingers[current_finger].orientation = slot->orientation;
    fingers[current_finger].position_x = slot->position_x;
    fingers[current_finger].position_y = slot->position_y;
    fingers[current_finger].tracking_id = slot->tracking_id;
    current_finger++;
  }
  hwstate.touch_cnt = Event_Get_Touch_Count(evdev);
  hwstate.finger_cnt = current_finger;
  hwstate.fingers = fingers;

  // Buttons.
  if (Event_Get_Button_Left(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_LEFT;
  if (Event_Get_Button_Middle(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_MIDDLE;
  if (Event_Get_Button_Right(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_RIGHT;

  GestureInterpreterPushHardwareState(interpreter_, &hwstate);
}

void GestureInterpreterLibevdevCros::OnGestureReady(const Gesture* gesture) {
  switch (gesture->type) {
    case kGestureTypeMove:
      OnGestureMove(gesture, &gesture->details.move);
      break;
    case kGestureTypeScroll:
      OnGestureScroll(gesture, &gesture->details.scroll);
      break;
    case kGestureTypeButtonsChange:
      OnGestureButtonsChange(gesture, &gesture->details.buttons);
      break;
    case kGestureTypeContactInitiated:
    case kGestureTypeFling:
    case kGestureTypeSwipe:
    case kGestureTypeSwipeLift:
    case kGestureTypePinch:
    case kGestureTypeMetrics:
      // TODO(spang): Support remaining gestures.
      NOTIMPLEMENTED();
      break;
    default:
      LOG(WARNING) << base::StringPrintf("Unrecognized gesture type (%u)",
                                         gesture->type);
      break;
  }
}

void GestureInterpreterLibevdevCros::OnGestureMove(const Gesture* gesture,
                                                   const GestureMove* move) {
  DVLOG(3) << base::StringPrintf("Gesture Move: (%f, %f) [%f, %f]",
                                 move->dx,
                                 move->dy,
                                 move->ordinal_dx,
                                 move->ordinal_dy);
  if (!cursor_)
    return;  // No cursor!
  cursor_->MoveCursor(gfx::Vector2dF(move->dx, move->dy));
  // TODO(spang): Use move->ordinal_dx, move->ordinal_dy
  // TODO(spang): Use move->start_time, move->end_time
  MouseEvent event(ET_MOUSE_MOVED,
                   cursor_->location(),
                   cursor_->location(),
                   modifiers_->GetModifierFlags(),
                   /* changed_button_flags */ 0);
  Dispatch(&event);
}

void GestureInterpreterLibevdevCros::OnGestureScroll(
    const Gesture* gesture,
    const GestureScroll* scroll) {
  if (!cursor_)
    return;  // No cursor!
  DVLOG(3) << base::StringPrintf("Gesture Scroll: (%f, %f) [%f, %f]",
                                 scroll->dx,
                                 scroll->dy,
                                 scroll->ordinal_dx,
                                 scroll->ordinal_dy);
  // TODO(spang): Support SetNaturalScroll
  // TODO(spang): Use scroll->start_time
  ScrollEvent event(ET_SCROLL,
                    cursor_->location(),
                    StimeToTimedelta(gesture->end_time),
                    modifiers_->GetModifierFlags(),
                    scroll->dx,
                    scroll->dy,
                    scroll->ordinal_dx,
                    scroll->ordinal_dy,
                    kGestureScrollFingerCount);
  Dispatch(&event);
}

void GestureInterpreterLibevdevCros::OnGestureButtonsChange(
    const Gesture* gesture,
    const GestureButtonsChange* buttons) {
  DVLOG(3) << base::StringPrintf("Gesture Button Change: down=0x%02x up=0x%02x",
                                 buttons->down,
                                 buttons->up);
  // TODO(spang): Use buttons->start_time, buttons->end_time
  if (buttons->down & GESTURES_BUTTON_LEFT)
    DispatchMouseButton(EVDEV_MODIFIER_LEFT_MOUSE_BUTTON, true);
  if (buttons->down & GESTURES_BUTTON_MIDDLE)
    DispatchMouseButton(EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON, true);
  if (buttons->down & GESTURES_BUTTON_RIGHT)
    DispatchMouseButton(EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON, true);
  if (buttons->up & GESTURES_BUTTON_LEFT)
    DispatchMouseButton(EVDEV_MODIFIER_LEFT_MOUSE_BUTTON, false);
  if (buttons->up & GESTURES_BUTTON_MIDDLE)
    DispatchMouseButton(EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON, false);
  if (buttons->up & GESTURES_BUTTON_RIGHT)
    DispatchMouseButton(EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON, false);
}

void GestureInterpreterLibevdevCros::Dispatch(Event* event) {
  dispatch_callback_.Run(event);
}

void GestureInterpreterLibevdevCros::DispatchMouseButton(unsigned int modifier,
                                                         bool down) {
  if (!cursor_)
    return;  // No cursor!
  const gfx::PointF& loc = cursor_->location();
  int flag = modifiers_->GetEventFlagFromModifier(modifier);
  EventType type = (down ? ET_MOUSE_PRESSED : ET_MOUSE_RELEASED);
  modifiers_->UpdateModifier(modifier, down);
  MouseEvent event(type, loc, loc, modifiers_->GetModifierFlags() | flag, flag);

  // This hack is necessary to trigger setting the repeat count.
  // TODO(spang): Fix it.
  MouseEvent event2(&event);
  Dispatch(&event2);
}

}  // namespace ui
