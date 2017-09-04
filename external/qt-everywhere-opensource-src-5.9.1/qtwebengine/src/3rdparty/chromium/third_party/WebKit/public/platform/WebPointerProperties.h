// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPointerProperties_h
#define WebPointerProperties_h

#include <cstdint>
#include <limits>

namespace blink {

// This class encapsulates the properties that are common between mouse and
// pointer events and touch points as we transition towards the unified pointer
// event model.
// TODO(e_hakkinen): Replace WebTouchEvent with WebPointerEvent, remove
// WebTouchEvent and WebTouchPoint and merge this into WebPointerEvent.
class WebPointerProperties {
 public:
  WebPointerProperties()
      : id(0),
        force(std::numeric_limits<float>::quiet_NaN()),
        tiltX(0),
        tiltY(0),
        button(Button::NoButton),
        pointerType(PointerType::Unknown) {}

  enum class Button { NoButton = -1, Left, Middle, Right, X1, X2, Eraser };

  enum class Buttons : unsigned {
    NoButton = 0,
    Left = 1 << 0,
    Right = 1 << 1,
    Middle = 1 << 2,
    X1 = 1 << 3,
    X2 = 1 << 4,
    Eraser = 1 << 5
  };

  enum class PointerType {
    Unknown,
    Mouse,
    Pen,
    Eraser,
    Touch,
    LastEntry = Touch  // Must be the last entry in the list
  };

  int id;

  // The valid range is [0,1], with NaN meaning pressure is not supported by
  // the input device.
  float force;

  // Tilt of a pen stylus from surface normal as plane angles in degrees,
  // Values lie in [-90,90]. A positive tiltX is to the right and a positive
  // tiltY is towards the user.
  int tiltX;
  int tiltY;

  Button button;
  PointerType pointerType;
};

}  // namespace blink

#endif
