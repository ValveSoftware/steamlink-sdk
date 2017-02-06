// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRPose.h"

namespace blink {

namespace {

DOMFloat32Array* mojoArrayToFloat32Array(const mojo::WTFArray<float>& vec)
{
    if (vec.is_null())
        return nullptr;

    return DOMFloat32Array::create(&(vec.front()), vec.size());
}

} // namespace

VRPose::VRPose()
    : m_timeStamp(0.0)
{
}

void VRPose::setPose(const device::blink::VRPosePtr& state)
{
    if (state.is_null())
        return;

    m_timeStamp = state->timestamp;
    m_orientation = mojoArrayToFloat32Array(state->orientation);
    m_position = mojoArrayToFloat32Array(state->position);
    m_angularVelocity = mojoArrayToFloat32Array(state->angularVelocity);
    m_linearVelocity = mojoArrayToFloat32Array(state->linearVelocity);
    m_angularAcceleration = mojoArrayToFloat32Array(state->angularAcceleration);
    m_linearAcceleration = mojoArrayToFloat32Array(state->linearAcceleration);
}

DEFINE_TRACE(VRPose)
{
    visitor->trace(m_orientation);
    visitor->trace(m_position);
    visitor->trace(m_angularVelocity);
    visitor->trace(m_linearVelocity);
    visitor->trace(m_angularAcceleration);
    visitor->trace(m_linearAcceleration);
}

} // namespace blink
