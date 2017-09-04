// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadPose_h
#define GamepadPose_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTypedArray.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepad.h"
#include "wtf/Forward.h"

namespace blink {

class GamepadPose final : public GarbageCollected<GamepadPose>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadPose* create() { return new GamepadPose(); }

  bool hasOrientation() const { return m_hasOrientation; }
  bool hasPosition() const { return m_hasPosition; }

  DOMFloat32Array* orientation() const { return m_orientation; }
  DOMFloat32Array* position() const { return m_position; }
  DOMFloat32Array* angularVelocity() const { return m_angularVelocity; }
  DOMFloat32Array* linearVelocity() const { return m_linearVelocity; }
  DOMFloat32Array* angularAcceleration() const { return m_angularAcceleration; }
  DOMFloat32Array* linearAcceleration() const { return m_linearAcceleration; }

  void setPose(const WebGamepadPose& state);

  DECLARE_VIRTUAL_TRACE();

 private:
  GamepadPose();

  bool m_hasOrientation;
  bool m_hasPosition;

  Member<DOMFloat32Array> m_orientation;
  Member<DOMFloat32Array> m_position;
  Member<DOMFloat32Array> m_angularVelocity;
  Member<DOMFloat32Array> m_linearVelocity;
  Member<DOMFloat32Array> m_angularAcceleration;
  Member<DOMFloat32Array> m_linearAcceleration;
};

}  // namespace blink

#endif  // VRPose_h
