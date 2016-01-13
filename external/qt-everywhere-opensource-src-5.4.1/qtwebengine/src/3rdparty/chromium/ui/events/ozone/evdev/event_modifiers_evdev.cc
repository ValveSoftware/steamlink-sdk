// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_modifiers_evdev.h"

#include <linux/input.h>

#include "ui/events/event.h"

namespace ui {

namespace {

static const int kEventFlagFromModifiers[] = {
    EF_NONE,                 // EVDEV_MODIFIER_NONE,
    EF_CAPS_LOCK_DOWN,       // EVDEV_MODIFIER_CAPS_LOCK
    EF_SHIFT_DOWN,           // EVDEV_MODIFIER_SHIFT
    EF_CONTROL_DOWN,         // EVDEV_MODIFIER_CONTROL
    EF_ALT_DOWN,             // EVDEV_MODIFIER_ALT
    EF_LEFT_MOUSE_BUTTON,    // EVDEV_MODIFIER_LEFT_MOUSE_BUTTON
    EF_MIDDLE_MOUSE_BUTTON,  // EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON
    EF_RIGHT_MOUSE_BUTTON,   // EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON
    EF_COMMAND_DOWN,         // EVDEV_MODIFIER_COMMAND
    EF_ALTGR_DOWN,           // EVDEV_MODIFIER_ALTGR
};

}  // namespace

EventModifiersEvdev::EventModifiersEvdev()
    : modifier_flags_locked_(0), modifier_flags_(0) {
  memset(modifiers_down_, 0, sizeof(modifiers_down_));
}
EventModifiersEvdev::~EventModifiersEvdev() {}

void EventModifiersEvdev::UpdateModifier(unsigned int modifier, bool down) {
  CHECK_LT(modifier, EVDEV_NUM_MODIFIERS);

  if (down) {
    modifiers_down_[modifier]++;
  } else {
    // Ignore spurious modifier "up" events. This might happen if the
    // button is down during startup.
    if (modifiers_down_[modifier])
      modifiers_down_[modifier]--;
  }

  UpdateFlags(modifier);
}

void EventModifiersEvdev::UpdateModifierLock(unsigned int modifier, bool down) {
  CHECK_LT(modifier, EVDEV_NUM_MODIFIERS);

  if (down)
    modifier_flags_locked_ ^= kEventFlagFromModifiers[modifier];

  // TODO(spang): Synchronize with the CapsLock LED.

  UpdateFlags(modifier);
}

void EventModifiersEvdev::UpdateFlags(unsigned int modifier) {
  int mask = kEventFlagFromModifiers[modifier];
  bool down = modifiers_down_[modifier];
  bool locked = (modifier_flags_locked_ & mask);
  if (down != locked)
    modifier_flags_ |= mask;
  else
    modifier_flags_ &= ~mask;
}

int EventModifiersEvdev::GetModifierFlags() { return modifier_flags_; }

// static
int EventModifiersEvdev::GetEventFlagFromModifier(unsigned int modifier) {
  return kEventFlagFromModifiers[modifier];
}

}  // namespace ui
