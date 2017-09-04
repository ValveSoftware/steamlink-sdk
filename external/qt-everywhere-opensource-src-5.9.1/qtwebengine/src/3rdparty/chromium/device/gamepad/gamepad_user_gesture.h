// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_USER_GESTURE_H_
#define DEVICE_GAMEPAD_USER_GESTURE_H_

namespace blink {
class WebGamepads;
}

namespace device {

// Returns true if any of the gamepads have a button pressed or axis moved
// that would be considered a user gesture for interaction.
bool GamepadsHaveUserGesture(const blink::WebGamepads& gamepads);

}  // namespace device

#endif  // DEVICE_GAMEPAD_USER_GESTURE_H_
