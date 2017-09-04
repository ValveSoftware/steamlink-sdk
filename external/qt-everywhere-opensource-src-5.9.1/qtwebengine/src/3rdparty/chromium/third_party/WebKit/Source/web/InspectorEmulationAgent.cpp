// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorEmulationAgent.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebViewScheduler.h"
#include "web/DevToolsEmulator.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace EmulationAgentState {
static const char scriptExecutionDisabled[] = "scriptExecutionDisabled";
static const char touchEventEmulationEnabled[] = "touchEventEmulationEnabled";
static const char emulatedMedia[] = "emulatedMedia";
static const char forcedViewportEnabled[] = "forcedViewportEnabled";
static const char forcedViewportX[] = "forcedViewportX";
static const char forcedViewportY[] = "forcedViewportY";
static const char forcedViewportScale[] = "forcedViewportScale";
}

InspectorEmulationAgent* InspectorEmulationAgent::create(
    WebLocalFrameImpl* webLocalFrameImpl,
    Client* client) {
  return new InspectorEmulationAgent(webLocalFrameImpl, client);
}

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* webLocalFrameImpl,
    Client* client)
    : m_webLocalFrameImpl(webLocalFrameImpl),
      m_client(client),
      m_virtualTimeBudgetExpiredTask(CancellableTaskFactory::create(
          this,
          &InspectorEmulationAgent::virtualTimeBudgetExpired)) {}

InspectorEmulationAgent::~InspectorEmulationAgent() {}

WebViewImpl* InspectorEmulationAgent::webViewImpl() {
  return m_webLocalFrameImpl->viewImpl();
}

void InspectorEmulationAgent::restore() {
  setScriptExecutionDisabled(m_state->booleanProperty(
      EmulationAgentState::scriptExecutionDisabled, false));
  setTouchEmulationEnabled(
      m_state->booleanProperty(EmulationAgentState::touchEventEmulationEnabled,
                               false),
      protocol::Maybe<String>());
  String emulatedMedia;
  m_state->getString(EmulationAgentState::emulatedMedia, &emulatedMedia);
  setEmulatedMedia(emulatedMedia);
  if (m_state->booleanProperty(EmulationAgentState::forcedViewportEnabled,
                               false)) {
    forceViewport(

        m_state->doubleProperty(EmulationAgentState::forcedViewportX, 0),
        m_state->doubleProperty(EmulationAgentState::forcedViewportY, 0),
        m_state->doubleProperty(EmulationAgentState::forcedViewportScale, 1));
  }
}

Response InspectorEmulationAgent::disable() {
  setScriptExecutionDisabled(false);
  setTouchEmulationEnabled(false, Maybe<String>());
  setEmulatedMedia(String());
  setCPUThrottlingRate(1);
  resetViewport();
  return Response::OK();
}

Response InspectorEmulationAgent::forceViewport(double x,
                                                double y,
                                                double scale) {
  if (x < 0 || y < 0)
    return Response::Error("Coordinates must be non-negative");

  if (scale <= 0)
    return Response::Error("Scale must be positive");

  m_state->setBoolean(EmulationAgentState::forcedViewportEnabled, true);
  m_state->setDouble(EmulationAgentState::forcedViewportX, x);
  m_state->setDouble(EmulationAgentState::forcedViewportY, y);
  m_state->setDouble(EmulationAgentState::forcedViewportScale, scale);

  webViewImpl()->devToolsEmulator()->forceViewport(WebFloatPoint(x, y), scale);
  return Response::OK();
}

Response InspectorEmulationAgent::resetViewport() {
  m_state->setBoolean(EmulationAgentState::forcedViewportEnabled, false);
  webViewImpl()->devToolsEmulator()->resetViewport();
  return Response::OK();
}

Response InspectorEmulationAgent::resetPageScaleFactor() {
  webViewImpl()->resetScaleStateImmediately();
  return Response::OK();
}

Response InspectorEmulationAgent::setPageScaleFactor(double pageScaleFactor) {
  webViewImpl()->setPageScaleFactor(static_cast<float>(pageScaleFactor));
  return Response::OK();
}

Response InspectorEmulationAgent::setScriptExecutionDisabled(bool value) {
  m_state->setBoolean(EmulationAgentState::scriptExecutionDisabled, value);
  webViewImpl()->devToolsEmulator()->setScriptExecutionDisabled(value);
  return Response::OK();
}

Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    Maybe<String> configuration) {
  m_state->setBoolean(EmulationAgentState::touchEventEmulationEnabled, enabled);
  webViewImpl()->devToolsEmulator()->setTouchEventEmulationEnabled(enabled);
  return Response::OK();
}

Response InspectorEmulationAgent::setEmulatedMedia(const String& media) {
  m_state->setString(EmulationAgentState::emulatedMedia, media);
  webViewImpl()->page()->settings().setMediaTypeOverride(media);
  return Response::OK();
}

Response InspectorEmulationAgent::setCPUThrottlingRate(double throttlingRate) {
  m_client->setCPUThrottlingRate(throttlingRate);
  return Response::OK();
}

Response InspectorEmulationAgent::setVirtualTimePolicy(const String& policy,
                                                       Maybe<int> budget) {
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::ADVANCE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::Pause == policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::PAUSE);
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
        WebViewScheduler::VirtualTimePolicy::DETERMINISTIC_LOADING);
  }
  m_webLocalFrameImpl->view()->scheduler()->enableVirtualTime();

  if (budget.isJust()) {
    WebTaskRunner* taskRunner =
        Platform::current()->currentThread()->getWebTaskRunner();
    long long delayMillis = static_cast<long long>(budget.fromJust());
    taskRunner->postDelayedTask(
        BLINK_FROM_HERE, m_virtualTimeBudgetExpiredTask->cancelAndCreate(),
        delayMillis);
  }
  return Response::OK();
}

void InspectorEmulationAgent::virtualTimeBudgetExpired() {
  m_webLocalFrameImpl->view()->scheduler()->setVirtualTimePolicy(
      WebViewScheduler::VirtualTimePolicy::PAUSE);
  frontend()->virtualTimeBudgetExpired();
}

DEFINE_TRACE(InspectorEmulationAgent) {
  visitor->trace(m_webLocalFrameImpl);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
