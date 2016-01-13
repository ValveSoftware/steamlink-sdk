// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#endif

#include <cmath>
#include <cstring>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

#if defined(USE_X11)
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#elif defined(USE_OZONE)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

namespace {

std::string EventTypeName(ui::EventType type) {
#define RETURN_IF_TYPE(t) if (type == ui::t)  return #t
#define CASE_TYPE(t) case ui::t:  return #t
  switch (type) {
    CASE_TYPE(ET_UNKNOWN);
    CASE_TYPE(ET_MOUSE_PRESSED);
    CASE_TYPE(ET_MOUSE_DRAGGED);
    CASE_TYPE(ET_MOUSE_RELEASED);
    CASE_TYPE(ET_MOUSE_MOVED);
    CASE_TYPE(ET_MOUSE_ENTERED);
    CASE_TYPE(ET_MOUSE_EXITED);
    CASE_TYPE(ET_KEY_PRESSED);
    CASE_TYPE(ET_KEY_RELEASED);
    CASE_TYPE(ET_MOUSEWHEEL);
    CASE_TYPE(ET_MOUSE_CAPTURE_CHANGED);
    CASE_TYPE(ET_TOUCH_RELEASED);
    CASE_TYPE(ET_TOUCH_PRESSED);
    CASE_TYPE(ET_TOUCH_MOVED);
    CASE_TYPE(ET_TOUCH_CANCELLED);
    CASE_TYPE(ET_DROP_TARGET_EVENT);
    CASE_TYPE(ET_TRANSLATED_KEY_PRESS);
    CASE_TYPE(ET_TRANSLATED_KEY_RELEASE);
    CASE_TYPE(ET_GESTURE_SCROLL_BEGIN);
    CASE_TYPE(ET_GESTURE_SCROLL_END);
    CASE_TYPE(ET_GESTURE_SCROLL_UPDATE);
    CASE_TYPE(ET_GESTURE_SHOW_PRESS);
    CASE_TYPE(ET_GESTURE_WIN8_EDGE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP);
    CASE_TYPE(ET_GESTURE_TAP_DOWN);
    CASE_TYPE(ET_GESTURE_TAP_CANCEL);
    CASE_TYPE(ET_GESTURE_BEGIN);
    CASE_TYPE(ET_GESTURE_END);
    CASE_TYPE(ET_GESTURE_TWO_FINGER_TAP);
    CASE_TYPE(ET_GESTURE_PINCH_BEGIN);
    CASE_TYPE(ET_GESTURE_PINCH_END);
    CASE_TYPE(ET_GESTURE_PINCH_UPDATE);
    CASE_TYPE(ET_GESTURE_LONG_PRESS);
    CASE_TYPE(ET_GESTURE_LONG_TAP);
    CASE_TYPE(ET_GESTURE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP_UNCONFIRMED);
    CASE_TYPE(ET_GESTURE_DOUBLE_TAP);
    CASE_TYPE(ET_SCROLL);
    CASE_TYPE(ET_SCROLL_FLING_START);
    CASE_TYPE(ET_SCROLL_FLING_CANCEL);
    CASE_TYPE(ET_CANCEL_MODE);
    CASE_TYPE(ET_UMA_DATA);
    case ui::ET_LAST: NOTREACHED(); return std::string();
    // Don't include default, so that we get an error when new type is added.
  }
#undef CASE_TYPE

  NOTREACHED();
  return std::string();
}

bool IsX11SendEventTrue(const base::NativeEvent& event) {
#if defined(USE_X11)
  return event && event->xany.send_event;
#else
  return false;
#endif
}

bool X11EventHasNonStandardState(const base::NativeEvent& event) {
#if defined(USE_X11)
  const unsigned int kAllStateMask =
      Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask |
      LockMask | ControlMask | AnyModifier;
  return event && (event->xkey.state & ~kAllStateMask) != 0;
#else
  return false;
#endif
}

}  // namespace

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// Event

Event::~Event() {
  if (delete_native_event_)
    ReleaseCopiedNativeEvent(native_event_);
}

bool Event::HasNativeEvent() const {
  base::NativeEvent null_event;
  std::memset(&null_event, 0, sizeof(null_event));
  return !!std::memcmp(&native_event_, &null_event, sizeof(null_event));
}

void Event::StopPropagation() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_CONSUMED);
}

void Event::SetHandled() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_HANDLED);
}

Event::Event(EventType type, base::TimeDelta time_stamp, int flags)
    : type_(type),
      time_stamp_(time_stamp),
      flags_(flags),
      native_event_(base::NativeEvent()),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED) {
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

Event::Event(const base::NativeEvent& native_event,
             EventType type,
             int flags)
    : type_(type),
      time_stamp_(EventTimeFromNative(native_event)),
      flags_(flags),
      native_event_(native_event),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED) {
  base::TimeDelta delta = EventTimeForNow() - time_stamp_;
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser",
                              delta.InMicroseconds(), 1, 1000000, 100);
  std::string name_for_event =
      base::StringPrintf("Event.Latency.Browser.%s", name_.c_str());
  base::HistogramBase* counter_for_type =
      base::Histogram::FactoryGet(
          name_for_event,
          1,
          1000000,
          100,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter_for_type->Add(delta.InMicroseconds());
}

Event::Event(const Event& copy)
    : type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      latency_(copy.latency_),
      flags_(copy.flags_),
      native_event_(CopyNativeEvent(copy.native_event_)),
      delete_native_event_(true),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED) {
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

void Event::SetType(EventType type) {
  if (type_ < ET_LAST)
    name_ = std::string();
  type_ = type;
  if (type_ < ET_LAST)
    name_ = EventTypeName(type_);
}

////////////////////////////////////////////////////////////////////////////////
// CancelModeEvent

CancelModeEvent::CancelModeEvent()
    : Event(ET_CANCEL_MODE, base::TimeDelta(), 0) {
  set_cancelable(false);
}

CancelModeEvent::~CancelModeEvent() {
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent

LocatedEvent::~LocatedEvent() {
}

LocatedEvent::LocatedEvent(const base::NativeEvent& native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      location_(EventLocationFromNative(native_event)),
      root_location_(location_) {
}

LocatedEvent::LocatedEvent(EventType type,
                           const gfx::PointF& location,
                           const gfx::PointF& root_location,
                           base::TimeDelta time_stamp,
                           int flags)
    : Event(type, time_stamp, flags),
      location_(location),
      root_location_(root_location) {
}

void LocatedEvent::UpdateForRootTransform(
    const gfx::Transform& reversed_root_transform) {
  // Transform has to be done at root level.
  gfx::Point3F p(location_);
  reversed_root_transform.TransformPoint(&p);
  location_ = p.AsPointF();
  root_location_ = location_;
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent

MouseEvent::MouseEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      changed_button_flags_(
          GetChangedMouseButtonFlagsFromNative(native_event)) {
  if (type() == ET_MOUSE_PRESSED || type() == ET_MOUSE_RELEASED)
    SetClickCount(GetRepeatCount(*this));
}

MouseEvent::MouseEvent(EventType type,
                       const gfx::PointF& location,
                       const gfx::PointF& root_location,
                       int flags,
                       int changed_button_flags)
    : LocatedEvent(type, location, root_location, EventTimeForNow(), flags),
      changed_button_flags_(changed_button_flags) {
  if (this->type() == ET_MOUSE_MOVED && IsAnyButton())
    SetType(ET_MOUSE_DRAGGED);
}

// static
bool MouseEvent::IsRepeatedClickEvent(
    const MouseEvent& event1,
    const MouseEvent& event2) {
  // These values match the Windows defaults.
  static const int kDoubleClickTimeMS = 500;
  static const int kDoubleClickWidth = 4;
  static const int kDoubleClickHeight = 4;

  if (event1.type() != ET_MOUSE_PRESSED ||
      event2.type() != ET_MOUSE_PRESSED)
    return false;

  // Compare flags, but ignore EF_IS_DOUBLE_CLICK to allow triple clicks.
  if ((event1.flags() & ~EF_IS_DOUBLE_CLICK) !=
      (event2.flags() & ~EF_IS_DOUBLE_CLICK))
    return false;

  base::TimeDelta time_difference = event2.time_stamp() - event1.time_stamp();

  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return false;

  if (std::abs(event2.x() - event1.x()) > kDoubleClickWidth / 2)
    return false;

  if (std::abs(event2.y() - event1.y()) > kDoubleClickHeight / 2)
    return false;

  return true;
}

// static
int MouseEvent::GetRepeatCount(const MouseEvent& event) {
  int click_count = 1;
  if (last_click_event_) {
    if (event.type() == ui::ET_MOUSE_RELEASED) {
      if (event.changed_button_flags() ==
              last_click_event_->changed_button_flags()) {
        last_click_complete_ = true;
        return last_click_event_->GetClickCount();
      } else {
        // If last_click_event_ has changed since this button was pressed
        // return a click count of 1.
        return click_count;
      }
    }
    if (event.time_stamp() != last_click_event_->time_stamp())
      last_click_complete_ = true;
    if (!last_click_complete_ ||
        IsX11SendEventTrue(event.native_event())) {
      click_count = last_click_event_->GetClickCount();
    } else if (IsRepeatedClickEvent(*last_click_event_, event)) {
      click_count = last_click_event_->GetClickCount() + 1;
    }
    delete last_click_event_;
  }
  last_click_event_ = new MouseEvent(event);
  last_click_complete_ = false;
  if (click_count > 3)
    click_count = 3;
  last_click_event_->SetClickCount(click_count);
  return click_count;
}

void MouseEvent::ResetLastClickForTest() {
  if (last_click_event_) {
    delete last_click_event_;
    last_click_event_ = NULL;
    last_click_complete_ = false;
  }
}

// static
MouseEvent* MouseEvent::last_click_event_ = NULL;
bool MouseEvent::last_click_complete_ = false;

int MouseEvent::GetClickCount() const {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return 0;

  if (flags() & EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags() & EF_IS_DOUBLE_CLICK)
    return 2;
  else
    return 1;
}

void MouseEvent::SetClickCount(int click_count) {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return;

  DCHECK(click_count > 0);
  DCHECK(click_count <= 3);

  int f = flags();
  switch (click_count) {
    case 1:
      f &= ~EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 2:
      f |= EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 3:
      f &= ~EF_IS_DOUBLE_CLICK;
      f |= EF_IS_TRIPLE_CLICK;
      break;
  }
  set_flags(f);
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent

MouseWheelEvent::MouseWheelEvent(const base::NativeEvent& native_event)
    : MouseEvent(native_event),
      offset_(GetMouseWheelOffset(native_event)) {
}

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event),
      offset_(scroll_event.x_offset(), scroll_event.y_offset()){
  SetType(ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseEvent& mouse_event,
                                 int x_offset,
                                 int y_offset)
    : MouseEvent(mouse_event), offset_(x_offset, y_offset) {
  DCHECK(type() == ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseWheelEvent& mouse_wheel_event)
    : MouseEvent(mouse_wheel_event),
      offset_(mouse_wheel_event.offset()) {
  DCHECK(type() == ET_MOUSEWHEEL);
}

#if defined(OS_WIN)
// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;
#else
// This value matches GTK+ wheel scroll amount.
const int MouseWheelEvent::kWheelDelta = 53;
#endif

void MouseWheelEvent::UpdateForRootTransform(
    const gfx::Transform& inverted_root_transform) {
  LocatedEvent::UpdateForRootTransform(inverted_root_transform);
  gfx::DecomposedTransform decomp;
  bool success = gfx::DecomposeTransform(&decomp, inverted_root_transform);
  DCHECK(success);
  if (decomp.scale[0])
    offset_.set_x(offset_.x() * decomp.scale[0]);
  if (decomp.scale[1])
    offset_.set_y(offset_.y() * decomp.scale[1]);
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent

TouchEvent::TouchEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(GetTouchId(native_event)),
      radius_x_(GetTouchRadiusX(native_event)),
      radius_y_(GetTouchRadiusY(native_event)),
      rotation_angle_(GetTouchAngle(native_event)),
      force_(GetTouchForce(native_event)),
      source_device_id_(-1) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      0,
      0,
      base::TimeTicks::FromInternalValue(time_stamp().ToInternalValue()),
      1);

#if defined(USE_X11)
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  source_device_id_ = xiev->deviceid;
#endif

  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::PointF& location,
                       int touch_id,
                       base::TimeDelta time_stamp)
    : LocatedEvent(type, location, location, time_stamp, 0),
      touch_id_(touch_id),
      radius_x_(0.0f),
      radius_y_(0.0f),
      rotation_angle_(0.0f),
      force_(0.0f),
      source_device_id_(-1) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::PointF& location,
                       int flags,
                       int touch_id,
                       base::TimeDelta time_stamp,
                       float radius_x,
                       float radius_y,
                       float angle,
                       float force)
    : LocatedEvent(type, location, location, time_stamp, flags),
      touch_id_(touch_id),
      radius_x_(radius_x),
      radius_y_(radius_y),
      rotation_angle_(angle),
      force_(force),
      source_device_id_(-1) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);
}

TouchEvent::~TouchEvent() {
  // In ctor TouchEvent(native_event) we call GetTouchId() which in X11
  // platform setups the tracking_id to slot mapping. So in dtor here,
  // if this touch event is a release event, we clear the mapping accordingly.
  if (HasNativeEvent())
    ClearTouchIdIfReleased(native_event());
}

void TouchEvent::UpdateForRootTransform(
    const gfx::Transform& inverted_root_transform) {
  LocatedEvent::UpdateForRootTransform(inverted_root_transform);
  gfx::DecomposedTransform decomp;
  bool success = gfx::DecomposeTransform(&decomp, inverted_root_transform);
  DCHECK(success);
  if (decomp.scale[0])
    radius_x_ *= decomp.scale[0];
  if (decomp.scale[1])
    radius_y_ *= decomp.scale[1];
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent

// static
KeyEvent* KeyEvent::last_key_event_ = NULL;

// static
bool KeyEvent::IsRepeated(const KeyEvent& event) {
  // A safe guard in case if there were continous key pressed events that are
  // not auto repeat.
  const int kMaxAutoRepeatTimeMs = 2000;
  // Ignore key events that have non standard state masks as it may be
  // reposted by an IME. IBUS-GTK uses this field to detect the
  // re-posted event for example. crbug.com/385873.
  if (X11EventHasNonStandardState(event.native_event()))
    return false;
  if (event.is_char())
    return false;
  if (event.type() == ui::ET_KEY_RELEASED) {
    delete last_key_event_;
    last_key_event_ = NULL;
    return false;
  }
  CHECK_EQ(ui::ET_KEY_PRESSED, event.type());
  if (!last_key_event_) {
    last_key_event_ = new KeyEvent(event);
    return false;
  }
  if (event.key_code() == last_key_event_->key_code() &&
      event.flags() == last_key_event_->flags() &&
      (event.time_stamp() - last_key_event_->time_stamp()).InMilliseconds() <
      kMaxAutoRepeatTimeMs) {
    return true;
  }
  delete last_key_event_;
  last_key_event_ = new KeyEvent(event);
  return false;
}

KeyEvent::KeyEvent(const base::NativeEvent& native_event, bool is_char)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      key_code_(KeyboardCodeFromNative(native_event)),
      code_(CodeFromNative(native_event)),
      is_char_(is_char),
      platform_keycode_(PlatformKeycodeFromNative(native_event)),
      character_(0) {
  if (IsRepeated(*this))
    set_flags(flags() | ui::EF_IS_REPEAT);

#if defined(USE_X11)
  NormalizeFlags();
#endif
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   int flags,
                   bool is_char)
    : Event(type, EventTimeForNow(), flags),
      key_code_(key_code),
      is_char_(is_char),
      platform_keycode_(0),
      character_(GetCharacterFromKeyCode(key_code, flags)) {
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   const std::string& code,
                   int flags,
                   bool is_char)
    : Event(type, EventTimeForNow(), flags),
      key_code_(key_code),
      code_(code),
      is_char_(is_char),
      platform_keycode_(0),
      character_(GetCharacterFromKeyCode(key_code, flags)) {
}

uint16 KeyEvent::GetCharacter() const {
  if (character_)
    return character_;

#if defined(OS_WIN)
  return (native_event().message == WM_CHAR) ? key_code_ :
      GetCharacterFromKeyCode(key_code_, flags());
#elif defined(USE_X11)
  if (!native_event())
    return GetCharacterFromKeyCode(key_code_, flags());

  DCHECK(native_event()->type == KeyPress ||
         native_event()->type == KeyRelease);

  // When a control key is held, prefer ASCII characters to non ASCII
  // characters in order to use it for shortcut keys.  GetCharacterFromKeyCode
  // returns 'a' for VKEY_A even if the key is actually bound to 'à' in X11.
  // GetCharacterFromXEvent returns 'à' in that case.
  return IsControlDown() ?
      GetCharacterFromKeyCode(key_code_, flags()) :
      GetCharacterFromXEvent(native_event());
#else
  if (native_event()) {
    DCHECK(EventTypeFromNative(native_event()) == ET_KEY_PRESSED ||
           EventTypeFromNative(native_event()) == ET_KEY_RELEASED);
  }

  return GetCharacterFromKeyCode(key_code_, flags());
#endif
}

bool KeyEvent::IsUnicodeKeyCode() const {
#if defined(OS_WIN)
  if (!IsAltDown())
    return false;
  const int key = key_code();
  if (key >= VKEY_NUMPAD0 && key <= VKEY_NUMPAD9)
    return true;
  // Check whether the user is using the numeric keypad with num-lock off.
  // In that case, EF_EXTENDED will not be set; if it is set, the key event
  // originated from the relevant non-numpad dedicated key, e.g. [Insert].
  return (!(flags() & EF_EXTENDED) &&
          (key == VKEY_INSERT || key == VKEY_END  || key == VKEY_DOWN ||
           key == VKEY_NEXT   || key == VKEY_LEFT || key == VKEY_CLEAR ||
           key == VKEY_RIGHT  || key == VKEY_HOME || key == VKEY_UP ||
           key == VKEY_PRIOR));
#else
  return false;
#endif
}

void KeyEvent::NormalizeFlags() {
  int mask = 0;
  switch (key_code()) {
    case VKEY_CONTROL:
      mask = EF_CONTROL_DOWN;
      break;
    case VKEY_SHIFT:
      mask = EF_SHIFT_DOWN;
      break;
    case VKEY_MENU:
      mask = EF_ALT_DOWN;
      break;
    case VKEY_CAPITAL:
      mask = EF_CAPS_LOCK_DOWN;
      break;
    default:
      return;
  }
  if (type() == ET_KEY_PRESSED)
    set_flags(flags() | mask);
  else
    set_flags(flags() & ~mask);
}

bool KeyEvent::IsTranslated() const {
  switch (type()) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      return false;
    case ET_TRANSLATED_KEY_PRESS:
    case ET_TRANSLATED_KEY_RELEASE:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

void KeyEvent::SetTranslated(bool translated) {
  switch (type()) {
    case ET_KEY_PRESSED:
    case ET_TRANSLATED_KEY_PRESS:
      SetType(translated ? ET_TRANSLATED_KEY_PRESS : ET_KEY_PRESSED);
      break;
    case ET_KEY_RELEASED:
    case ET_TRANSLATED_KEY_RELEASE:
      SetType(translated ? ET_TRANSLATED_KEY_RELEASE : ET_KEY_RELEASED);
      break;
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent

ScrollEvent::ScrollEvent(const base::NativeEvent& native_event)
    : MouseEvent(native_event) {
  if (type() == ET_SCROLL) {
    GetScrollOffsets(native_event,
                     &x_offset_, &y_offset_,
                     &x_offset_ordinal_, &y_offset_ordinal_,
                     &finger_count_);
  } else if (type() == ET_SCROLL_FLING_START ||
             type() == ET_SCROLL_FLING_CANCEL) {
    GetFlingData(native_event,
                 &x_offset_, &y_offset_,
                 &x_offset_ordinal_, &y_offset_ordinal_,
                 NULL);
  } else {
    NOTREACHED() << "Unexpected event type " << type()
        << " when constructing a ScrollEvent.";
  }
}

ScrollEvent::ScrollEvent(EventType type,
                         const gfx::PointF& location,
                         base::TimeDelta time_stamp,
                         int flags,
                         float x_offset,
                         float y_offset,
                         float x_offset_ordinal,
                         float y_offset_ordinal,
                         int finger_count)
    : MouseEvent(type, location, location, flags, 0),
      x_offset_(x_offset),
      y_offset_(y_offset),
      x_offset_ordinal_(x_offset_ordinal),
      y_offset_ordinal_(y_offset_ordinal),
      finger_count_(finger_count) {
  set_time_stamp(time_stamp);
  CHECK(IsScrollEvent());
}

void ScrollEvent::Scale(const float factor) {
  x_offset_ *= factor;
  y_offset_ *= factor;
  x_offset_ordinal_ *= factor;
  y_offset_ordinal_ *= factor;
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent

GestureEvent::GestureEvent(EventType type,
                           float x,
                           float y,
                           int flags,
                           base::TimeDelta time_stamp,
                           const GestureEventDetails& details,
                           unsigned int touch_ids_bitfield)
    : LocatedEvent(type,
                   gfx::PointF(x, y),
                   gfx::PointF(x, y),
                   time_stamp,
                   flags | EF_FROM_TOUCH),
      details_(details),
      touch_ids_bitfield_(touch_ids_bitfield) {
}

GestureEvent::~GestureEvent() {
}

int GestureEvent::GetLowestTouchId() const {
  if (touch_ids_bitfield_ == 0)
    return -1;
  int i = -1;
  // Find the index of the least significant 1 bit
  while (!(1 << ++i & touch_ids_bitfield_));
  return i;
}

}  // namespace ui
