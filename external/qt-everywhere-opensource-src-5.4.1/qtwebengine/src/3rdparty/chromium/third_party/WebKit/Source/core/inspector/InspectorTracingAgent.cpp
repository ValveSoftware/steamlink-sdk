//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "config.h"

#include "core/inspector/InspectorTracingAgent.h"

#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorState.h"
#include "platform/TraceEvent.h"

namespace WebCore {

namespace TracingAgentState {
const char sessionId[] = "sessionId";
}

namespace {
const char devtoolsMetadataEventCategory[] = TRACE_DISABLED_BY_DEFAULT("devtools.timeline");
}

InspectorTracingAgent::InspectorTracingAgent(InspectorClient* client)
    : InspectorBaseAgent<InspectorTracingAgent>("Tracing")
    , m_layerTreeId(0)
    , m_client(client)
{
}

void InspectorTracingAgent::restore()
{
    emitMetadataEvents();
}

void InspectorTracingAgent::start(ErrorString*, const String& categoryFilter, const String&, const double*, String* outSessionId)
{
    m_state->setString(TracingAgentState::sessionId, IdentifiersFactory::createIdentifier());
    m_client->enableTracing(categoryFilter);
    emitMetadataEvents();
    *outSessionId = sessionId();
}

String InspectorTracingAgent::sessionId()
{
    return m_state->getString(TracingAgentState::sessionId);
}

void InspectorTracingAgent::emitMetadataEvents()
{
    TRACE_EVENT_INSTANT1(devtoolsMetadataEventCategory, "TracingStartedInPage", "sessionId", sessionId().utf8());
    if (m_layerTreeId)
        setLayerTreeId(m_layerTreeId);
}

void InspectorTracingAgent::setLayerTreeId(int layerTreeId)
{
    m_layerTreeId = layerTreeId;
    TRACE_EVENT_INSTANT2(devtoolsMetadataEventCategory, "SetLayerTreeId", "sessionId", sessionId().utf8(), "layerTreeId", m_layerTreeId);
}

}
