// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_DELEGATE_H_
#define COMPONENTS_EXO_KEYBOARD_DELEGATE_H_

#include <vector>

#include "base/time/time.h"

namespace ui {
enum class DomCode;
}

namespace exo {
class Keyboard;
class Surface;

// Handles events on keyboards in context-specific ways.
class KeyboardDelegate {
 public:
  // Called at the top of the keyboard's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnKeyboardDestroying(Keyboard* keyboard) = 0;

  // This should return true if |surface| is a valid target for this keyboard.
  // E.g. the surface is owned by the same client as the keyboard.
  virtual bool CanAcceptKeyboardEventsForSurface(Surface* surface) const = 0;

  // Called when keyboard focus enters a new valid target surface.
  virtual void OnKeyboardEnter(
      Surface* surface,
      const std::vector<ui::DomCode>& pressed_keys) = 0;

  // Called when keyboard focus leaves a valid target surface.
  virtual void OnKeyboardLeave(Surface* surface) = 0;

  // Called when pkeyboard key state changed. |pressed| is true when |key|
  // was pressed and false if it was released.
  virtual void OnKeyboardKey(base::TimeTicks time_stamp,
                             ui::DomCode key,
                             bool pressed) = 0;

  // Called when keyboard modifier state changed.
  virtual void OnKeyboardModifiers(int modifier_flags) = 0;

 protected:
  virtual ~KeyboardDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_DELEGATE_H_
