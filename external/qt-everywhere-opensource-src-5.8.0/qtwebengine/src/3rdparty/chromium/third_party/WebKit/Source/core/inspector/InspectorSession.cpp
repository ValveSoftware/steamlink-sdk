// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorSession.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/inspector_protocol/Parser.h"
#include "platform/v8_inspector/protocol/Debugger.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
}

InspectorSession::InspectorSession(Client* client, InspectedFrames* inspectedFrames, InstrumentingAgents* instrumentingAgents, int sessionId, bool autoFlush, V8Debugger* debugger, int contextGroupId, const String* savedState)
    : m_client(client)
    , m_v8Session(nullptr)
    , m_sessionId(sessionId)
    , m_autoFlush(autoFlush)
    , m_disposed(false)
    , m_inspectedFrames(inspectedFrames)
    , m_instrumentingAgents(instrumentingAgents)
    , m_inspectorBackendDispatcher(new protocol::UberDispatcher(this))
{
    InspectorInstrumentation::frontendCreated();

    if (savedState) {
        std::unique_ptr<protocol::Value> state = protocol::parseJSON(*savedState);
        if (state)
            m_state = protocol::DictionaryValue::cast(std::move(state));
        if (!m_state)
            m_state = protocol::DictionaryValue::create();
    } else {
        m_state = protocol::DictionaryValue::create();
    }

    String16 v8State;
    m_state->getString(kV8StateKey, &v8State);
    m_v8Session = debugger->connect(contextGroupId, this, this, savedState ? &v8State : nullptr);
}

InspectorSession::~InspectorSession()
{
    DCHECK(m_disposed);
}

void InspectorSession::append(InspectorAgent* agent)
{
    m_agents.append(agent);
    agent->init(m_instrumentingAgents.get(), m_inspectorBackendDispatcher.get(), m_state.get());
}

void InspectorSession::restore()
{
    DCHECK(!m_disposed);
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->restore();
}

void InspectorSession::dispose()
{
    DCHECK(!m_disposed);
    m_disposed = true;
    m_inspectorBackendDispatcher.reset();
    for (size_t i = m_agents.size(); i > 0; i--)
        m_agents[i - 1]->dispose();
    m_agents.clear();
    m_v8Session.reset();
    DCHECK(!isInstrumenting());
    InspectorInstrumentation::frontendDeleted();
}

void InspectorSession::dispatchProtocolMessage(const String& method, const String& message)
{
    DCHECK(!m_disposed);
    if (V8InspectorSession::isV8ProtocolMethod(method))
        m_v8Session->dispatchProtocolMessage(message);
    else
        m_inspectorBackendDispatcher->dispatch(message);
}

void InspectorSession::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->didCommitLoadForLocalFrame(frame);
}

void InspectorSession::sendProtocolResponse(int callId, const protocol::String16& message)
{
    if (m_disposed)
        return;
    flushProtocolNotifications();
    m_state->setString(kV8StateKey, m_v8Session->stateJSON());
    String stateToSend = m_state->toJSONString();
    if (stateToSend == m_lastSentState)
        stateToSend = String();
    else
        m_lastSentState = stateToSend;
    m_client->sendProtocolMessage(m_sessionId, callId, message, stateToSend);
}

void InspectorSession::sendProtocolNotification(const protocol::String16& message)
{
    if (m_disposed)
        return;
    if (m_autoFlush)
        m_client->sendProtocolMessage(m_sessionId, 0, message, String());
    else
        m_notificationQueue.append(message);
}

void InspectorSession::flushProtocolNotifications()
{
    if (m_disposed)
        return;
    for (size_t i = 0; i < m_agents.size(); i++)
        m_agents[i]->flushPendingProtocolNotifications();
    for (size_t i = 0; i < m_notificationQueue.size(); ++i)
        m_client->sendProtocolMessage(m_sessionId, 0, m_notificationQueue[i], String());
    m_notificationQueue.clear();
}

void InspectorSession::scriptExecutionBlockedByCSP(const String& directiveText)
{
    DCHECK(isInstrumenting());
    std::unique_ptr<protocol::DictionaryValue> directive = protocol::DictionaryValue::create();
    directive->setString("directiveText", directiveText);
    m_v8Session->breakProgramOnException(protocol::Debugger::Paused::ReasonEnum::CSPViolation, std::move(directive));
}

void InspectorSession::didStartProvisionalLoad(LocalFrame* frame)
{
    DCHECK(isInstrumenting());
    if (m_inspectedFrames && m_inspectedFrames->root() == frame)
        m_v8Session->resume();
}

void InspectorSession::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    DCHECK(isInstrumenting());
    frame->script().initializeMainWorld();
}

void InspectorSession::startInstrumenting()
{
    DCHECK(!isInstrumenting());
    m_instrumentingAgents->addInspectorSession(this);
    forceContextsInAllFrames();
}

void InspectorSession::stopInstrumenting()
{
    DCHECK(isInstrumenting());
    m_instrumentingAgents->removeInspectorSession(this);
}

void InspectorSession::resumeStartup()
{
    m_client->resumeStartup();
}

bool InspectorSession::canExecuteScripts()
{
    return m_inspectedFrames ? m_inspectedFrames->root()->script().canExecuteScripts(NotAboutToExecuteScript) : true;
}

void InspectorSession::profilingStarted()
{
    m_client->profilingStarted();
}

void InspectorSession::profilingStopped()
{
    m_client->profilingStopped();
}

void InspectorSession::consoleEnabled()
{
    m_client->consoleEnabled();
}

void InspectorSession::consoleCleared()
{
    m_client->consoleCleared();
}

void InspectorSession::forceContextsInAllFrames()
{
    if (!m_inspectedFrames)
        return;
    if (!m_inspectedFrames->root()->loader().stateMachine()->committedFirstRealDocumentLoad())
        return;
    for (const LocalFrame* frame : *m_inspectedFrames)
        frame->script().initializeMainWorld();
}

bool InspectorSession::isInstrumenting()
{
    return m_instrumentingAgents->inspectorSessions().contains(this);
}

DEFINE_TRACE(InspectorSession)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_agents);
}

} // namespace blink

