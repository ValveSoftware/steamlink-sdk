// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorEmulationAgent.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "web/DevToolsEmulator.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace EmulationAgentState {
static const char scriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char touchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char emulatedMedia[] = "emulatedMedia";
}

InspectorEmulationAgent* InspectorEmulationAgent::create(WebLocalFrameImpl* webLocalFrameImpl, Client* client)
{
    return new InspectorEmulationAgent(webLocalFrameImpl, client);
}

InspectorEmulationAgent::InspectorEmulationAgent(WebLocalFrameImpl* webLocalFrameImpl, Client* client)
    : m_webLocalFrameImpl(webLocalFrameImpl)
    , m_client(client)
{
}

InspectorEmulationAgent::~InspectorEmulationAgent()
{
}

WebViewImpl* InspectorEmulationAgent::webViewImpl()
{
    return m_webLocalFrameImpl->viewImpl();
}

void InspectorEmulationAgent::restore()
{
    ErrorString error;
    setScriptExecutionDisabled(&error, m_state->booleanProperty(EmulationAgentState::scriptExecutionDisabled, false));
    setTouchEmulationEnabled(&error, m_state->booleanProperty(EmulationAgentState::touchEventEmulationEnabled, false), protocol::Maybe<String>());
    String16 emulatedMedia;
    m_state->getString(EmulationAgentState::emulatedMedia, &emulatedMedia);
    setEmulatedMedia(&error, emulatedMedia);
}

void InspectorEmulationAgent::disable(ErrorString*)
{
    ErrorString error;
    setScriptExecutionDisabled(&error, false);
    setTouchEmulationEnabled(&error, false, protocol::Maybe<String>());
    setEmulatedMedia(&error, String());
}

void InspectorEmulationAgent::resetPageScaleFactor(ErrorString*)
{
    webViewImpl()->resetScaleStateImmediately();
}

void InspectorEmulationAgent::setPageScaleFactor(ErrorString*, double pageScaleFactor)
{
    webViewImpl()->setPageScaleFactor(static_cast<float>(pageScaleFactor));
}

void InspectorEmulationAgent::setScriptExecutionDisabled(ErrorString*, bool value)
{
    m_state->setBoolean(EmulationAgentState::scriptExecutionDisabled, value);
    webViewImpl()->devToolsEmulator()->setScriptExecutionDisabled(value);
}

void InspectorEmulationAgent::setTouchEmulationEnabled(ErrorString*, bool enabled, const Maybe<String>& configuration)
{
    m_state->setBoolean(EmulationAgentState::touchEventEmulationEnabled, enabled);
    webViewImpl()->devToolsEmulator()->setTouchEventEmulationEnabled(enabled);
}

void InspectorEmulationAgent::setEmulatedMedia(ErrorString*, const String& media)
{
    m_state->setString(EmulationAgentState::emulatedMedia, media);
    webViewImpl()->page()->settings().setMediaTypeOverride(media);
}

void InspectorEmulationAgent::setCPUThrottlingRate(ErrorString*, double throttlingRate)
{
    m_client->setCPUThrottlingRate(throttlingRate);
}

DEFINE_TRACE(InspectorEmulationAgent)
{
    visitor->trace(m_webLocalFrameImpl);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
