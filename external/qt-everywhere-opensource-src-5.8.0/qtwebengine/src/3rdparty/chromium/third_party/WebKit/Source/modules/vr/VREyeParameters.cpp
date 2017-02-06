// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VREyeParameters.h"

namespace blink {

VREyeParameters::VREyeParameters()
{
    m_offset = DOMFloat32Array::create(3);
    m_fieldOfView = new VRFieldOfView();
    m_renderWidth = 0;
    m_renderHeight = 0;
}

void VREyeParameters::update(const device::blink::VREyeParametersPtr& eyeParameters)
{
    m_offset->data()[0] = eyeParameters->offset[0];
    m_offset->data()[1] = eyeParameters->offset[1];
    m_offset->data()[2] = eyeParameters->offset[2];

    m_fieldOfView->setUpDegrees(eyeParameters->fieldOfView->upDegrees);
    m_fieldOfView->setDownDegrees(eyeParameters->fieldOfView->downDegrees);
    m_fieldOfView->setLeftDegrees(eyeParameters->fieldOfView->leftDegrees);
    m_fieldOfView->setRightDegrees(eyeParameters->fieldOfView->rightDegrees);

    m_renderWidth = eyeParameters->renderWidth;
    m_renderHeight = eyeParameters->renderHeight;
}

DEFINE_TRACE(VREyeParameters)
{
    visitor->trace(m_offset);
    visitor->trace(m_fieldOfView);
}

} // namespace blink
