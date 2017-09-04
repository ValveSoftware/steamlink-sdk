//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "core/inspector/InspectorTracingAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

namespace TracingAgentState {
const char sessionId[] = "sessionId";
}

namespace {
const char devtoolsMetadataEventCategory[] =
    TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(Client* client,
                                             InspectorWorkerAgent* workerAgent,
                                             InspectedFrames* inspectedFrames)
    : m_layerTreeId(0),
      m_client(client),
      m_workerAgent(workerAgent),
      m_inspectedFrames(inspectedFrames) {}

DEFINE_TRACE(InspectorTracingAgent) {
  visitor->trace(m_workerAgent);
  visitor->trace(m_inspectedFrames);
  InspectorBaseAgent::trace(visitor);
}

void InspectorTracingAgent::restore() {
  emitMetadataEvents();
}

void InspectorTracingAgent::frameStartedLoading(LocalFrame* frame) {
  if (frame != m_inspectedFrames->root() ||
      frame->loader().loadType() != FrameLoadTypeReload)
    return;
  m_client->showReloadingBlanket();
}

void InspectorTracingAgent::frameStoppedLoading(LocalFrame* frame) {
  if (frame != m_inspectedFrames->root())
    m_client->hideReloadingBlanket();
}

void InspectorTracingAgent::start(Maybe<String> categories,
                                  Maybe<String> options,
                                  Maybe<double> bufferUsageReportingInterval,
                                  Maybe<String> transferMode,
                                  Maybe<protocol::Tracing::TraceConfig> config,
                                  std::unique_ptr<StartCallback> callback) {
  DCHECK(!isStarted());
  if (config.isJust()) {
    callback->sendFailure(Response::Error(
        "Using trace config on renderer targets is not supported yet."));
    return;
  }

  m_instrumentingAgents->addInspectorTracingAgent(this);
  m_state->setString(TracingAgentState::sessionId,
                     IdentifiersFactory::createIdentifier());
  m_client->enableTracing(categories.fromMaybe(String()));
  emitMetadataEvents();
  callback->sendSuccess();
}

void InspectorTracingAgent::end(std::unique_ptr<EndCallback> callback) {
  m_client->disableTracing();
  innerDisable();
  callback->sendSuccess();
}

bool InspectorTracingAgent::isStarted() const {
  return !sessionId().isEmpty();
}

String InspectorTracingAgent::sessionId() const {
  String result;
  if (m_state)
    m_state->getString(TracingAgentState::sessionId, &result);
  return result;
}

void InspectorTracingAgent::emitMetadataEvents() {
  TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "TracingStartedInPage",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTracingStartedInFrame::data(
                           sessionId(), m_inspectedFrames->root()));
  if (m_layerTreeId)
    setLayerTreeId(m_layerTreeId);
  m_workerAgent->setTracingSessionId(sessionId());
}

void InspectorTracingAgent::setLayerTreeId(int layerTreeId) {
  m_layerTreeId = layerTreeId;
  TRACE_EVENT_INSTANT1(
      devtoolsMetadataEventCategory, "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD,
      "data", InspectorSetLayerTreeId::data(sessionId(), m_layerTreeId));
}

void InspectorTracingAgent::rootLayerCleared() {
  if (isStarted())
    m_client->hideReloadingBlanket();
}

Response InspectorTracingAgent::disable() {
  innerDisable();
  return Response::OK();
}

void InspectorTracingAgent::innerDisable() {
  m_client->hideReloadingBlanket();
  m_instrumentingAgents->removeInspectorTracingAgent(this);
  m_state->remove(TracingAgentState::sessionId);
  m_workerAgent->setTracingSessionId(String());
}

}  // namespace blink
