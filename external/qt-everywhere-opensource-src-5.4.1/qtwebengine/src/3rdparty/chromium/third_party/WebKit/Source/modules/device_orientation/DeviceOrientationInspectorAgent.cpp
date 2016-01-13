// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorState.h"
#include "core/page/Page.h"

#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"

namespace WebCore {

namespace DeviceOrientationInspectorAgentState {
static const char alpha[] = "alpha";
static const char beta[] = "beta";
static const char gamma[] = "gamma";
static const char overrideEnabled[] = "overrideEnabled";
}

void DeviceOrientationInspectorAgent::provideTo(Page& page)
{
    OwnPtr<DeviceOrientationInspectorAgent> deviceOrientationAgent(adoptPtr(new DeviceOrientationInspectorAgent(page)));
    page.inspectorController().registerModuleAgent(deviceOrientationAgent.release());
}

DeviceOrientationInspectorAgent::~DeviceOrientationInspectorAgent()
{
}

DeviceOrientationInspectorAgent::DeviceOrientationInspectorAgent(Page& page)
    : InspectorBaseAgent<DeviceOrientationInspectorAgent>("DeviceOrientation")
    , m_page(page)
{
}

DeviceOrientationController& DeviceOrientationInspectorAgent::controller()
{
    ASSERT(toLocalFrame(m_page.mainFrame())->document());
    return DeviceOrientationController::from(*m_page.deprecatedLocalMainFrame()->document());
}

void DeviceOrientationInspectorAgent::setDeviceOrientationOverride(ErrorString* error, double alpha, double beta, double gamma)
{
    m_state->setBoolean(DeviceOrientationInspectorAgentState::overrideEnabled, true);
    m_state->setDouble(DeviceOrientationInspectorAgentState::alpha, alpha);
    m_state->setDouble(DeviceOrientationInspectorAgentState::beta, beta);
    m_state->setDouble(DeviceOrientationInspectorAgentState::gamma, gamma);
    controller().setOverride(DeviceOrientationData::create(true, alpha, true, beta, true, gamma).get());
}

void DeviceOrientationInspectorAgent::clearDeviceOrientationOverride(ErrorString* error)
{
    m_state->setBoolean(DeviceOrientationInspectorAgentState::overrideEnabled, false);
    controller().clearOverride();
}

void DeviceOrientationInspectorAgent::clearFrontend()
{
    m_state->setBoolean(DeviceOrientationInspectorAgentState::overrideEnabled, false);
    controller().clearOverride();
}

void DeviceOrientationInspectorAgent::restore()
{
    if (m_state->getBoolean(DeviceOrientationInspectorAgentState::overrideEnabled)) {
        double alpha = m_state->getDouble(DeviceOrientationInspectorAgentState::alpha);
        double beta = m_state->getDouble(DeviceOrientationInspectorAgentState::beta);
        double gamma = m_state->getDouble(DeviceOrientationInspectorAgentState::gamma);
        controller().setOverride(DeviceOrientationData::create(true, alpha, true, beta, true, gamma).get());
    }
}

void DeviceOrientationInspectorAgent::didCommitLoadForMainFrame()
{
    // New document in main frame - apply override there.
    // No need to cleanup previous one, as it's already gone.
    restore();
}

} // namespace WebCore
