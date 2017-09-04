// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_DELEGATE_H_
#define COMPONENTS_EXO_GAMEPAD_DELEGATE_H_

#include "base/memory/weak_ptr.h"

namespace exo {
class Gamepad;
class Surface;

// Handles events on multiple gamepad
class GamepadDelegate {
 public:
  // Gives the delegate a chance to clean up when the Gamepad instance is
  // destroyed.
  virtual void OnGamepadDestroying(Gamepad* gamepad) = 0;

  // This should return true if |surface| is a valid target for this gamepad.
  // E.g. the surface is owned by the same client as the gamepad.
  virtual bool CanAcceptGamepadEventsForSurface(Surface* surface) const = 0;

  // Called when the state of the gamepad has changed.
  virtual void OnStateChange(bool connected) = 0;

  // Called when the user moved an axis of the gamepad. Valid axes are defined
  // by the W3C 'standard gamepad' specification.
  virtual void OnAxis(int axis, double value) = 0;

  // Called when the user pressed or moved a button of the gamepad.
  // Valid buttons are defined by the W3C 'standard gamepad' specification.
  virtual void OnButton(int button, bool pressed, double value) = 0;

  // Called after all gamepad information of this frame has been set and the
  // client should evaluate the updated state.
  virtual void OnFrame() = 0;

 protected:
  virtual ~GamepadDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_DELEGATE_H_
