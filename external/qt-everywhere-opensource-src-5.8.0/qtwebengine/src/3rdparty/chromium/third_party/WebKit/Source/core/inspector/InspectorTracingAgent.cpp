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
#include "platform/TraceEvent.h"

namespace blink {

namespace TracingAgentState {
const char sessionId[] = "sessionId";
}

namespace {
const char devtoolsMetadataEventCategory[] = TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(Client* client, InspectorWorkerAgent* workerAgent, InspectedFrames* inspectedFrames)
    : m_layerTreeId(0)
    , m_client(client)
    , m_workerAgent(workerAgent)
    , m_inspectedFrames(inspectedFrames)
{
}

DEFINE_TRACE(InspectorTracingAgent)
{
    visitor->trace(m_workerAgent);
    visitor->trace(m_inspectedFrames);
    InspectorBaseAgent::trace(visitor);
}

void InspectorTracingAgent::restore()
{
    emitMetadataEvents();
}
void InspectorTracingAgent::start(ErrorString* errorString,
    const Maybe<String>& categories,
    const Maybe<String>& options,
    const Maybe<double>& bufferUsageReportingInterval,
    const Maybe<String>& transferMode,
    const Maybe<protocol::Tracing::TraceConfig>& config,
    std::unique_ptr<StartCallback> callback)
{
    ASSERT(sessionId().isEmpty());
    if (config.isJust()) {
        *errorString =
            "Using trace config on renderer targets is not supported yet.";
        return;
    }

    m_state->setString(TracingAgentState::sessionId, IdentifiersFactory::createIdentifier());
    m_client->enableTracing(categories.fromMaybe(String()));
    emitMetadataEvents();
    callback->sendSuccess();
}

void InspectorTracingAgent::end(ErrorString* errorString, std::unique_ptr<EndCallback> callback)
{
    m_client->disableTracing();
    resetSessionId();
    callback->sendSuccess();
}

String InspectorTracingAgent::sessionId()
{
    String16 result;
    if (m_state)
        m_state->getString(TracingAgentState::sessionId, &result);
    return result;
}

void InspectorTracingAgent::emitMetadataEvents()
{
    TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "TracingStartedInPage", TRACE_EVENT_SCOPE_THREAD, "data", InspectorTracingStartedInFrame::data(sessionId(), m_inspectedFrames->root()));
    if (m_layerTreeId)
        setLayerTreeId(m_layerTreeId);
    m_workerAgent->setTracingSessionId(sessionId());
}

void InspectorTracingAgent::setLayerTreeId(int layerTreeId)
{
    m_layerTreeId = layerTreeId;
    TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD, "data", InspectorSetLayerTreeId::data(sessionId(), m_layerTreeId));
}

void InspectorTracingAgent::disable(ErrorString*)
{
    resetSessionId();
}

void InspectorTracingAgent::resetSessionId()
{
    m_state->remove(TracingAgentState::sessionId);
    m_workerAgent->setTracingSessionId(String());
}

} // namespace blink
