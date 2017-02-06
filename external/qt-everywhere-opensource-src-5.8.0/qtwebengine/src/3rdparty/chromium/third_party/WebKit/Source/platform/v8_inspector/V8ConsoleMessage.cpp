// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8ConsoleMessage.h"

#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/V8ConsoleAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

namespace {

String messageSourceValue(MessageSource source)
{
    switch (source) {
    case XMLMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Xml;
    case JSMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Javascript;
    case NetworkMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Network;
    case ConsoleAPIMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::ConsoleApi;
    case StorageMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Storage;
    case AppCacheMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Appcache;
    case RenderingMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Rendering;
    case SecurityMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Security;
    case OtherMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Other;
    case DeprecationMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Deprecation;
    }
    return protocol::Console::ConsoleMessage::SourceEnum::Other;
}


String messageTypeValue(MessageType type)
{
    switch (type) {
    case LogMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    case ClearMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Clear;
    case DirMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Dir;
    case DirXMLMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Dirxml;
    case TableMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Table;
    case TraceMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Trace;
    case StartGroupMessageType: return protocol::Console::ConsoleMessage::TypeEnum::StartGroup;
    case StartGroupCollapsedMessageType: return protocol::Console::ConsoleMessage::TypeEnum::StartGroupCollapsed;
    case EndGroupMessageType: return protocol::Console::ConsoleMessage::TypeEnum::EndGroup;
    case AssertMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Assert;
    case TimeEndMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    case CountMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    }
    return protocol::Console::ConsoleMessage::TypeEnum::Log;
}

String messageLevelValue(MessageLevel level)
{
    switch (level) {
    case DebugMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Debug;
    case LogMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Log;
    case WarningMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Warning;
    case ErrorMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Error;
    case InfoMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Info;
    case RevokedErrorMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::RevokedError;
    }
    return protocol::Console::ConsoleMessage::LevelEnum::Log;
}

const unsigned maxConsoleMessageCount = 1000;
const unsigned maxArrayItemsLimit = 10000;
const unsigned maxStackDepthLimit = 32;

class V8ValueStringBuilder {
public:
    static String16 toString(v8::Local<v8::Value> value, v8::Isolate* isolate)
    {
        V8ValueStringBuilder builder(isolate);
        if (!builder.append(value))
            return String16();
        return builder.toString();
    }

private:
    enum {
        IgnoreNull = 1 << 0,
        IgnoreUndefined = 1 << 1,
    };

    V8ValueStringBuilder(v8::Isolate* isolate)
        : m_arrayLimit(maxArrayItemsLimit)
        , m_isolate(isolate)
        , m_tryCatch(isolate)
    {
    }

    bool append(v8::Local<v8::Value> value, unsigned ignoreOptions = 0)
    {
        if (value.IsEmpty())
            return true;
        if ((ignoreOptions & IgnoreNull) && value->IsNull())
            return true;
        if ((ignoreOptions & IgnoreUndefined) && value->IsUndefined())
            return true;
        if (value->IsString())
            return append(v8::Local<v8::String>::Cast(value));
        if (value->IsStringObject())
            return append(v8::Local<v8::StringObject>::Cast(value)->ValueOf());
        if (value->IsSymbol())
            return append(v8::Local<v8::Symbol>::Cast(value));
        if (value->IsSymbolObject())
            return append(v8::Local<v8::SymbolObject>::Cast(value)->ValueOf());
        if (value->IsNumberObject()) {
            m_builder.appendNumber(v8::Local<v8::NumberObject>::Cast(value)->ValueOf());
            return true;
        }
        if (value->IsBooleanObject()) {
            m_builder.append(v8::Local<v8::BooleanObject>::Cast(value)->ValueOf() ? "true" : "false");
            return true;
        }
        if (value->IsArray())
            return append(v8::Local<v8::Array>::Cast(value));
        if (value->IsProxy()) {
            m_builder.append("[object Proxy]");
            return true;
        }
        if (value->IsObject()
            && !value->IsDate()
            && !value->IsFunction()
            && !value->IsNativeError()
            && !value->IsRegExp()) {
            v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
            v8::Local<v8::String> stringValue;
            if (object->ObjectProtoToString(m_isolate->GetCurrentContext()).ToLocal(&stringValue))
                return append(stringValue);
        }
        v8::Local<v8::String> stringValue;
        if (!value->ToString(m_isolate->GetCurrentContext()).ToLocal(&stringValue))
            return false;
        return append(stringValue);
    }

    bool append(v8::Local<v8::Array> array)
    {
        if (m_visitedArrays.contains(array))
            return true;
        uint32_t length = array->Length();
        if (length > m_arrayLimit)
            return false;
        if (m_visitedArrays.size() > maxStackDepthLimit)
            return false;

        bool result = true;
        m_arrayLimit -= length;
        m_visitedArrays.append(array);
        for (uint32_t i = 0; i < length; ++i) {
            if (i)
                m_builder.append(',');
            if (!append(array->Get(i), IgnoreNull | IgnoreUndefined)) {
                result = false;
                break;
            }
        }
        m_visitedArrays.removeLast();
        return result;
    }

    bool append(v8::Local<v8::Symbol> symbol)
    {
        m_builder.append("Symbol(");
        bool result = append(symbol->Name(), IgnoreUndefined);
        m_builder.append(')');
        return result;
    }

    bool append(v8::Local<v8::String> string)
    {
        if (m_tryCatch.HasCaught())
            return false;
        if (!string.IsEmpty())
            m_builder.append(toProtocolString(string));
        return true;
    }

    String16 toString()
    {
        if (m_tryCatch.HasCaught())
            return String16();
        return m_builder.toString();
    }

    uint32_t m_arrayLimit;
    v8::Isolate* m_isolate;
    String16Builder m_builder;
    Vector<v8::Local<v8::Array>> m_visitedArrays;
    v8::TryCatch m_tryCatch;
};

} // namespace

V8ConsoleMessage::V8ConsoleMessage(
    double timestampMS,
    MessageSource source,
    MessageLevel level,
    const String16& message,
    const String16& url,
    unsigned lineNumber,
    unsigned columnNumber,
    std::unique_ptr<V8StackTrace> stackTrace,
    int scriptId,
    const String16& requestIdentifier)
    : m_timestamp(timestampMS / 1000.0)
    , m_source(source)
    , m_level(level)
    , m_message(message)
    , m_url(url)
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_stackTrace(std::move(stackTrace))
    , m_scriptId(scriptId)
    , m_requestIdentifier(requestIdentifier)
    , m_contextId(0)
    , m_type(LogMessageType)
    , m_messageId(0)
    , m_relatedMessageId(0)
{
}

V8ConsoleMessage::~V8ConsoleMessage()
{
}

std::unique_ptr<protocol::Console::ConsoleMessage> V8ConsoleMessage::buildInspectorObject(V8InspectorSessionImpl* session, bool generatePreview) const
{
    std::unique_ptr<protocol::Console::ConsoleMessage> result =
        protocol::Console::ConsoleMessage::create()
        .setSource(messageSourceValue(m_source))
        .setLevel(messageLevelValue(m_level))
        .setText(m_message)
        .setTimestamp(m_timestamp)
        .build();
    result->setType(messageTypeValue(m_type));
    result->setLine(static_cast<int>(m_lineNumber));
    result->setColumn(static_cast<int>(m_columnNumber));
    if (m_scriptId)
        result->setScriptId(String::number(m_scriptId));
    result->setUrl(m_url);
    if (m_source == NetworkMessageSource && !m_requestIdentifier.isEmpty())
        result->setNetworkRequestId(m_requestIdentifier);
    if (m_contextId)
        result->setExecutionContextId(m_contextId);
    appendArguments(result.get(), session, generatePreview);
    if (m_stackTrace)
        result->setStack(m_stackTrace->buildInspectorObject());
    if (m_messageId)
        result->setMessageId(m_messageId);
    if (m_relatedMessageId)
        result->setRelatedMessageId(m_relatedMessageId);
    return result;
}

void V8ConsoleMessage::appendArguments(protocol::Console::ConsoleMessage* result, V8InspectorSessionImpl* session, bool generatePreview) const
{
    if (!m_arguments.size() || !m_contextId)
        return;
    InspectedContext* inspectedContext = session->debugger()->getContext(session->contextGroupId(), m_contextId);
    if (!inspectedContext)
        return;

    v8::Isolate* isolate = inspectedContext->isolate();
    v8::HandleScope handles(isolate);
    v8::Local<v8::Context> context = inspectedContext->context();

    std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>> args = protocol::Array<protocol::Runtime::RemoteObject>::create();
    if (m_type == TableMessageType && generatePreview) {
        v8::Local<v8::Value> table = m_arguments[0]->Get(isolate);
        v8::Local<v8::Value> columns = m_arguments.size() > 1 ? m_arguments[1]->Get(isolate) : v8::Local<v8::Value>();
        std::unique_ptr<protocol::Runtime::RemoteObject> wrapped = session->wrapTable(context, table, columns);
        if (wrapped)
            args->addItem(std::move(wrapped));
        else
            args = nullptr;
    } else {
        for (size_t i = 0; i < m_arguments.size(); ++i) {
            std::unique_ptr<protocol::Runtime::RemoteObject> wrapped = session->wrapObject(context, m_arguments[i]->Get(isolate), "console", generatePreview);
            if (!wrapped) {
                args = nullptr;
                break;
            }
            args->addItem(std::move(wrapped));
        }
    }
    if (args)
        result->setParameters(std::move(args));
}

unsigned V8ConsoleMessage::argumentCount() const
{
    return m_arguments.size();
}

MessageType V8ConsoleMessage::type() const
{
    return m_type;
}

// static
std::unique_ptr<V8ConsoleMessage> V8ConsoleMessage::createForConsoleAPI(double timestampMS, MessageType type, MessageLevel level, const String16& messageText, std::vector<v8::Local<v8::Value>>* arguments, std::unique_ptr<V8StackTrace> stackTrace, InspectedContext* context)
{
    v8::Isolate* isolate = context->isolate();
    int contextId = context->contextId();
    int contextGroupId = context->contextGroupId();
    V8DebuggerImpl* debugger = context->debugger();

    String16 url;
    unsigned lineNumber = 0;
    unsigned columnNumber = 0;
    if (stackTrace && !stackTrace->isEmpty()) {
        url = stackTrace->topSourceURL();
        lineNumber = stackTrace->topLineNumber();
        columnNumber = stackTrace->topColumnNumber();
    }

    String16 actualMessage = messageText;

    Arguments messageArguments;
    if (arguments && arguments->size()) {
        for (size_t i = 0; i < arguments->size(); ++i)
            messageArguments.push_back(wrapUnique(new v8::Global<v8::Value>(isolate, arguments->at(i))));
        if (actualMessage.isEmpty())
            actualMessage = V8ValueStringBuilder::toString(messageArguments.at(0)->Get(isolate), isolate);
    }

    std::unique_ptr<V8ConsoleMessage> message = wrapUnique(new V8ConsoleMessage(timestampMS, ConsoleAPIMessageSource, level, actualMessage, url, lineNumber, columnNumber, std::move(stackTrace), 0 /* scriptId */, String16() /* requestIdentifier */));
    message->m_type = type;
    if (messageArguments.size()) {
        message->m_contextId = contextId;
        message->m_arguments.swap(messageArguments);
    }

    debugger->client()->messageAddedToConsole(contextGroupId, message->m_source, message->m_level, message->m_message, message->m_url, message->m_lineNumber, message->m_columnNumber, message->m_stackTrace.get());
    return message;
}

void V8ConsoleMessage::contextDestroyed(int contextId)
{
    if (contextId != m_contextId)
        return;
    m_contextId = 0;
    if (m_message.isEmpty())
        m_message = "<message collected>";
    Arguments empty;
    m_arguments.swap(empty);
}

void V8ConsoleMessage::assignId(unsigned id)
{
    m_messageId = id;
}

void V8ConsoleMessage::assignRelatedId(unsigned id)
{
    m_relatedMessageId = id;
}

void V8ConsoleMessage::addArguments(v8::Isolate* isolate, int contextId, std::vector<v8::Local<v8::Value>>* arguments)
{
    if (!arguments || !contextId)
        return;
    m_contextId = contextId;
    for (size_t i = 0; i < arguments->size(); ++i)
        m_arguments.push_back(wrapUnique(new v8::Global<v8::Value>(isolate, arguments->at(i))));
}

// ------------------------ V8ConsoleMessageStorage ----------------------------

V8ConsoleMessageStorage::V8ConsoleMessageStorage(V8DebuggerImpl* debugger, int contextGroupId)
    : m_debugger(debugger)
    , m_contextGroupId(contextGroupId)
    , m_expiredCount(0)
{
}

V8ConsoleMessageStorage::~V8ConsoleMessageStorage()
{
    clear();
}

void V8ConsoleMessageStorage::addMessage(std::unique_ptr<V8ConsoleMessage> message)
{
    int contextGroupId = m_contextGroupId;
    V8DebuggerImpl* debugger = m_debugger;

    if (message->type() == ClearMessageType)
        clear();

    V8InspectorSessionImpl* session = debugger->sessionForContextGroup(contextGroupId);
    if (session)
        session->consoleAgent()->messageAdded(message.get());
    if (!debugger->hasConsoleMessageStorage(contextGroupId))
        return;

    DCHECK(m_messages.size() <= maxConsoleMessageCount);
    if (m_messages.size() == maxConsoleMessageCount) {
        ++m_expiredCount;
        m_messages.pop_front();
    }
    m_messages.push_back(std::move(message));
}

void V8ConsoleMessageStorage::clear()
{
    m_messages.clear();
    m_expiredCount = 0;
    V8InspectorSessionImpl* session = m_debugger->sessionForContextGroup(m_contextGroupId);
    if (session) {
        session->consoleAgent()->reset();
        session->releaseObjectGroup("console");
        session->client()->consoleCleared();
    }
}

void V8ConsoleMessageStorage::contextDestroyed(int contextId)
{
    for (size_t i = 0; i < m_messages.size(); ++i)
        m_messages[i]->contextDestroyed(contextId);
}

} // namespace blink
