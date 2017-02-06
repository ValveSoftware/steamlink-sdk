// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8ConsoleAgentImpl.h"

#include "platform/v8_inspector/V8ConsoleMessage.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"

namespace blink {

namespace ConsoleAgentState {
static const char consoleEnabled[] = "consoleEnabled";
}

V8ConsoleAgentImpl::V8ConsoleAgentImpl(V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel, protocol::DictionaryValue* state)
    : m_session(session)
    , m_state(state)
    , m_frontend(frontendChannel)
    , m_enabled(false)
{
}

V8ConsoleAgentImpl::~V8ConsoleAgentImpl()
{
}

void V8ConsoleAgentImpl::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;
    m_state->setBoolean(ConsoleAgentState::consoleEnabled, true);
    m_enabled = true;
    m_session->debugger()->enableStackCapturingIfNeeded();
    reportAllMessages();
    m_session->client()->consoleEnabled();
}

void V8ConsoleAgentImpl::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;
    m_session->debugger()->disableStackCapturingIfNeeded();
    m_state->setBoolean(ConsoleAgentState::consoleEnabled, false);
    m_enabled = false;
}

void V8ConsoleAgentImpl::clearMessages(ErrorString* errorString)
{
    m_session->debugger()->ensureConsoleMessageStorage(m_session->contextGroupId())->clear();
}

void V8ConsoleAgentImpl::restore()
{
    if (!m_state->booleanProperty(ConsoleAgentState::consoleEnabled, false))
        return;
    m_frontend.messagesCleared();
    ErrorString ignored;
    enable(&ignored);
}

void V8ConsoleAgentImpl::messageAdded(V8ConsoleMessage* message)
{
    if (m_enabled)
        reportMessage(message, true);
}

void V8ConsoleAgentImpl::reset()
{
    if (m_enabled)
        m_frontend.messagesCleared();
}

bool V8ConsoleAgentImpl::enabled()
{
    return m_enabled;
}

void V8ConsoleAgentImpl::reportAllMessages()
{
    V8ConsoleMessageStorage* storage = m_session->debugger()->ensureConsoleMessageStorage(m_session->contextGroupId());
    if (storage->expiredCount()) {
        std::unique_ptr<protocol::Console::ConsoleMessage> expired = protocol::Console::ConsoleMessage::create()
            .setSource(protocol::Console::ConsoleMessage::SourceEnum::Other)
            .setLevel(protocol::Console::ConsoleMessage::LevelEnum::Warning)
            .setText(String16::number(storage->expiredCount()) + String16("console messages are not shown."))
            .setTimestamp(0)
            .build();
        expired->setType(protocol::Console::ConsoleMessage::TypeEnum::Log);
        expired->setLine(0);
        expired->setColumn(0);
        expired->setUrl("");
        m_frontend.messageAdded(std::move(expired));
        m_frontend.flush();
    }
    for (const auto& message : storage->messages()) {
        if (!reportMessage(message.get(), false))
            return;
    }
}

bool V8ConsoleAgentImpl::reportMessage(V8ConsoleMessage* message, bool generatePreview)
{
    m_frontend.messageAdded(message->buildInspectorObject(m_session, generatePreview));
    m_frontend.flush();
    return m_session->debugger()->hasConsoleMessageStorage(m_session->contextGroupId());
}

} // namespace blink
