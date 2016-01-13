/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"


#include "core/inspector/ConsoleMessage.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

ConsoleMessage::ConsoleMessage(bool canGenerateCallStack, MessageSource source, MessageType type, MessageLevel level, const String& message)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_scriptState(0)
    , m_url()
    , m_line(0)
    , m_column(0)
    , m_requestId(IdentifiersFactory::requestId(0))
    , m_timestamp(WTF::currentTime())
{
    autogenerateMetadata(canGenerateCallStack);
}

ConsoleMessage::ConsoleMessage(bool canGenerateCallStack, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& url, unsigned line, unsigned column, ScriptState* scriptState, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_scriptState(scriptState)
    , m_url(url)
    , m_line(line)
    , m_column(column)
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
    , m_timestamp(WTF::currentTime())
{
    autogenerateMetadata(canGenerateCallStack, scriptState);
}

ConsoleMessage::ConsoleMessage(bool, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_scriptState(0)
    , m_arguments(nullptr)
    , m_line(0)
    , m_column(0)
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
    , m_timestamp(WTF::currentTime())
{
    if (callStack && callStack->size()) {
        const ScriptCallFrame& frame = callStack->at(0);
        m_url = frame.sourceURL();
        m_line = frame.lineNumber();
        m_column = frame.columnNumber();
    }
    m_callStack = callStack;
}

ConsoleMessage::ConsoleMessage(bool canGenerateCallStack, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtrWillBeRawPtr<ScriptArguments> arguments, ScriptState* scriptState, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_scriptState(scriptState)
    , m_arguments(arguments)
    , m_url()
    , m_line(0)
    , m_column(0)
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
    , m_timestamp(WTF::currentTime())
{
    autogenerateMetadata(canGenerateCallStack, scriptState);
}

ConsoleMessage::~ConsoleMessage()
{
}

void ConsoleMessage::autogenerateMetadata(bool canGenerateCallStack, ScriptState* scriptState)
{
    if (m_type == EndGroupMessageType)
        return;

    if (scriptState)
        m_callStack = createScriptCallStackForConsole(scriptState);
    else if (canGenerateCallStack)
        m_callStack = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);
    else
        return;

    if (m_callStack && m_callStack->size()) {
        const ScriptCallFrame& frame = m_callStack->at(0);
        m_url = frame.sourceURL();
        m_line = frame.lineNumber();
        m_column = frame.columnNumber();
        return;
    }

    m_callStack.clear();
}

static TypeBuilder::Console::ConsoleMessage::Source::Enum messageSourceValue(MessageSource source)
{
    switch (source) {
    case XMLMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Xml;
    case JSMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Javascript;
    case NetworkMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Network;
    case ConsoleAPIMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Console_api;
    case StorageMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Storage;
    case AppCacheMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Appcache;
    case RenderingMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Rendering;
    case CSSMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Css;
    case SecurityMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Security;
    case OtherMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Other;
    case DeprecationMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Deprecation;
    }
    return TypeBuilder::Console::ConsoleMessage::Source::Other;
}

static TypeBuilder::Console::ConsoleMessage::Type::Enum messageTypeValue(MessageType type)
{
    switch (type) {
    case LogMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Log;
    case ClearMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Clear;
    case DirMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Dir;
    case DirXMLMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Dirxml;
    case TableMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Table;
    case TraceMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Trace;
    case StartGroupMessageType: return TypeBuilder::Console::ConsoleMessage::Type::StartGroup;
    case StartGroupCollapsedMessageType: return TypeBuilder::Console::ConsoleMessage::Type::StartGroupCollapsed;
    case EndGroupMessageType: return TypeBuilder::Console::ConsoleMessage::Type::EndGroup;
    case AssertMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Assert;
    }
    return TypeBuilder::Console::ConsoleMessage::Type::Log;
}

static TypeBuilder::Console::ConsoleMessage::Level::Enum messageLevelValue(MessageLevel level)
{
    switch (level) {
    case DebugMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Debug;
    case LogMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Log;
    case WarningMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Warning;
    case ErrorMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Error;
    case InfoMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Info;
    }
    return TypeBuilder::Console::ConsoleMessage::Level::Log;
}

void ConsoleMessage::addToFrontend(InspectorFrontend::Console* frontend, InjectedScriptManager* injectedScriptManager, bool generatePreview)
{
    RefPtr<TypeBuilder::Console::ConsoleMessage> jsonObj = TypeBuilder::Console::ConsoleMessage::create()
        .setSource(messageSourceValue(m_source))
        .setLevel(messageLevelValue(m_level))
        .setText(m_message)
        .setTimestamp(m_timestamp);
    // FIXME: only send out type for ConsoleAPI source messages.
    jsonObj->setType(messageTypeValue(m_type));
    jsonObj->setLine(static_cast<int>(m_line));
    jsonObj->setColumn(static_cast<int>(m_column));
    jsonObj->setUrl(m_url);
    ScriptState* scriptState = m_scriptState.get();
    if (scriptState && scriptState->executionContext()->isDocument())
        jsonObj->setExecutionContextId(injectedScriptManager->injectedScriptIdFor(scriptState));
    if (m_source == NetworkMessageSource && !m_requestId.isEmpty())
        jsonObj->setNetworkRequestId(m_requestId);
    if (m_arguments && m_arguments->argumentCount()) {
        InjectedScript injectedScript = injectedScriptManager->injectedScriptFor(m_arguments->scriptState());
        if (!injectedScript.isEmpty()) {
            RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::RemoteObject> > jsonArgs = TypeBuilder::Array<TypeBuilder::Runtime::RemoteObject>::create();
            if (m_type == TableMessageType && generatePreview && m_arguments->argumentCount()) {
                ScriptValue table = m_arguments->argumentAt(0);
                ScriptValue columns = m_arguments->argumentCount() > 1 ? m_arguments->argumentAt(1) : ScriptValue();
                RefPtr<TypeBuilder::Runtime::RemoteObject> inspectorValue = injectedScript.wrapTable(table, columns);
                if (!inspectorValue) {
                    ASSERT_NOT_REACHED();
                    return;
                }
                jsonArgs->addItem(inspectorValue);
            } else {
                for (unsigned i = 0; i < m_arguments->argumentCount(); ++i) {
                    RefPtr<TypeBuilder::Runtime::RemoteObject> inspectorValue = injectedScript.wrapObject(m_arguments->argumentAt(i), "console", generatePreview);
                    if (!inspectorValue) {
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    jsonArgs->addItem(inspectorValue);
                }
            }
            jsonObj->setParameters(jsonArgs);
        }
    }
    if (m_callStack)
        jsonObj->setStackTrace(m_callStack->buildInspectorArray());
    frontend->messageAdded(jsonObj);
    frontend->flush();
}

void ConsoleMessage::windowCleared(LocalDOMWindow* window)
{
    if (m_scriptState.get() && m_scriptState.get()->domWindow() == window)
        m_scriptState.clear();

    if (!m_arguments)
        return;
    if (m_arguments->scriptState()->domWindow() != window)
        return;
    if (!m_message)
        m_message = "<message collected>";
    m_arguments.clear();
}

unsigned ConsoleMessage::argumentCount()
{
    if (m_arguments)
        return m_arguments->argumentCount();
    return 0;
}

} // namespace WebCore

