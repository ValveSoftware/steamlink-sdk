// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_device_info.h"

#include <linux/input.h>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

namespace ui {

namespace {

bool GetEventBits(int fd, unsigned int type, void* buf, unsigned int size) {
  if (ioctl(fd, EVIOCGBIT(type, size), buf) < 0) {
    DLOG(ERROR) << "failed EVIOCGBIT(" << type << ", " << size << ") on fd "
                << fd;
    return false;
  }

  return true;
}

bool GetPropBits(int fd, void* buf, unsigned int size) {
  if (ioctl(fd, EVIOCGPROP(size), buf) < 0) {
    DLOG(ERROR) << "failed EVIOCGPROP(" << size << ") on fd " << fd;
    return false;
  }

  return true;
}

bool BitIsSet(const unsigned long* bits, unsigned int bit) {
  return (bits[bit / EVDEV_LONG_BITS] & (1UL << (bit % EVDEV_LONG_BITS)));
}

bool GetAbsInfo(int fd, int code, struct input_absinfo* absinfo) {
  if (ioctl(fd, EVIOCGABS(code), absinfo)) {
    DLOG(ERROR) << "failed EVIOCGABS(" << code << ") on fd " << fd;
    return false;
  }
  return true;
}

}  // namespace

EventDeviceInfo::EventDeviceInfo() {
  memset(ev_bits_, 0, sizeof(ev_bits_));
  memset(key_bits_, 0, sizeof(key_bits_));
  memset(rel_bits_, 0, sizeof(rel_bits_));
  memset(abs_bits_, 0, sizeof(abs_bits_));
  memset(msc_bits_, 0, sizeof(msc_bits_));
  memset(sw_bits_, 0, sizeof(sw_bits_));
  memset(led_bits_, 0, sizeof(led_bits_));
  memset(prop_bits_, 0, sizeof(prop_bits_));
  memset(abs_info_, 0, sizeof(abs_info_));
}

EventDeviceInfo::~EventDeviceInfo() {}

bool EventDeviceInfo::Initialize(int fd) {
  if (!GetEventBits(fd, 0, ev_bits_, sizeof(ev_bits_)))
    return false;

  if (!GetEventBits(fd, EV_KEY, key_bits_, sizeof(key_bits_)))
    return false;

  if (!GetEventBits(fd, EV_REL, rel_bits_, sizeof(rel_bits_)))
    return false;

  if (!GetEventBits(fd, EV_ABS, abs_bits_, sizeof(abs_bits_)))
    return false;

  if (!GetEventBits(fd, EV_MSC, msc_bits_, sizeof(msc_bits_)))
    return false;

  if (!GetEventBits(fd, EV_SW, sw_bits_, sizeof(sw_bits_)))
    return false;

  if (!GetEventBits(fd, EV_LED, led_bits_, sizeof(led_bits_)))
    return false;

  if (!GetPropBits(fd, prop_bits_, sizeof(prop_bits_)))
    return false;

  for (unsigned int i = 0; i < ABS_CNT; ++i)
    if (HasAbsEvent(i))
      if (!GetAbsInfo(fd, i, &abs_info_[i]))
        return false;

  return true;
}

bool EventDeviceInfo::HasEventType(unsigned int type) const {
  if (type > EV_MAX)
    return false;
  return BitIsSet(ev_bits_, type);
}

bool EventDeviceInfo::HasKeyEvent(unsigned int code) const {
  if (code > KEY_MAX)
    return false;
  return BitIsSet(key_bits_, code);
}

bool EventDeviceInfo::HasRelEvent(unsigned int code) const {
  if (code > REL_MAX)
    return false;
  return BitIsSet(rel_bits_, code);
}

bool EventDeviceInfo::HasAbsEvent(unsigned int code) const {
  if (code > ABS_MAX)
    return false;
  return BitIsSet(abs_bits_, code);
}

bool EventDeviceInfo::HasMscEvent(unsigned int code) const {
  if (code > MSC_MAX)
    return false;
  return BitIsSet(msc_bits_, code);
}

bool EventDeviceInfo::HasSwEvent(unsigned int code) const {
  if (code > SW_MAX)
    return false;
  return BitIsSet(sw_bits_, code);
}

bool EventDeviceInfo::HasLedEvent(unsigned int code) const {
  if (code > LED_MAX)
    return false;
  return BitIsSet(led_bits_, code);
}

bool EventDeviceInfo::HasProp(unsigned int code) const {
  if (code > INPUT_PROP_MAX)
    return false;
  return BitIsSet(prop_bits_, code);
}

int32 EventDeviceInfo::GetAbsMinimum(unsigned int code) const {
  return abs_info_[code].minimum;
}

int32 EventDeviceInfo::GetAbsMaximum(unsigned int code) const {
  return abs_info_[code].maximum;
}

bool EventDeviceInfo::HasAbsXY() const {
  if (HasAbsEvent(ABS_X) && HasAbsEvent(ABS_Y))
    return true;

  if (HasAbsEvent(ABS_MT_POSITION_X) && HasAbsEvent(ABS_MT_POSITION_Y))
    return true;

  return false;
}

bool EventDeviceInfo::HasRelXY() const {
  return HasRelEvent(REL_X) && HasRelEvent(REL_Y);
}

bool EventDeviceInfo::IsMappedToScreen() const {
  // Device position is mapped directly to the screen.
  if (HasProp(INPUT_PROP_DIRECT))
    return true;

  // Device position moves the cursor.
  if (HasProp(INPUT_PROP_POINTER))
    return false;

  // Tablets are mapped to the screen.
  if (HasKeyEvent(BTN_TOOL_PEN) || HasKeyEvent(BTN_STYLUS) ||
      HasKeyEvent(BTN_STYLUS2))
    return true;

  // Touchpads are not mapped to the screen.
  if (HasKeyEvent(BTN_LEFT) || HasKeyEvent(BTN_MIDDLE) ||
      HasKeyEvent(BTN_RIGHT) || HasKeyEvent(BTN_TOOL_FINGER))
    return false;

  // Touchscreens are mapped to the screen.
  return true;
}

}  // namespace ui
