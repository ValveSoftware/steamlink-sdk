// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/gamepad/GamepadPose.h"

namespace blink {

namespace {

DOMFloat32Array* vecToFloat32Array(const WebGamepadVector& vec) {
  if (vec.notNull) {
    DOMFloat32Array* out = DOMFloat32Array::create(3);
    out->data()[0] = vec.x;
    out->data()[1] = vec.y;
    out->data()[2] = vec.z;
    return out;
  }
  return nullptr;
}

DOMFloat32Array* quatToFloat32Array(const WebGamepadQuaternion& quat) {
  if (quat.notNull) {
    DOMFloat32Array* out = DOMFloat32Array::create(4);
    out->data()[0] = quat.x;
    out->data()[1] = quat.y;
    out->data()[2] = quat.z;
    out->data()[3] = quat.w;
    return out;
  }
  return nullptr;
}

}  // namespace

GamepadPose::GamepadPose() {}

void GamepadPose::setPose(const WebGamepadPose& state) {
  if (state.notNull) {
    m_hasOrientation = state.hasOrientation;
    m_hasPosition = state.hasPosition;

    m_orientation = quatToFloat32Array(state.orientation);
    m_position = vecToFloat32Array(state.position);
    m_angularVelocity = vecToFloat32Array(state.angularVelocity);
    m_linearVelocity = vecToFloat32Array(state.linearVelocity);
    m_angularAcceleration = vecToFloat32Array(state.angularAcceleration);
    m_linearAcceleration = vecToFloat32Array(state.linearAcceleration);
  }
}

DEFINE_TRACE(GamepadPose) {
  visitor->trace(m_orientation);
  visitor->trace(m_position);
  visitor->trace(m_angularVelocity);
  visitor->trace(m_linearVelocity);
  visitor->trace(m_angularAcceleration);
  visitor->trace(m_linearAcceleration);
}

}  // namespace blink
