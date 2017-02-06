// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRStageParameters.h"

namespace blink {

VRStageParameters::VRStageParameters()
    : m_sizeX(0.0f)
    , m_sizeZ(0.0f)
{
    // Set the sitting to standing transform to identity matrix
    m_standingTransform = DOMFloat32Array::create(16);
    m_standingTransform->data()[0] = 1.0f;
    m_standingTransform->data()[5] = 1.0f;
    m_standingTransform->data()[10] = 1.0f;
    m_standingTransform->data()[15] = 1.0f;
}

void VRStageParameters::update(const device::blink::VRStageParametersPtr& stage)
{
    m_standingTransform = DOMFloat32Array::create(&(stage->standingTransform.front()), 16);
    m_sizeX = stage->sizeX;
    m_sizeZ = stage->sizeZ;
}

DEFINE_TRACE(VRStageParameters)
{
    visitor->trace(m_standingTransform);
}

} // namespace blink
