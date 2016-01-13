/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "core/frame/FrameConsole.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

namespace {

int muteCount = 0;

}

FrameConsole::FrameConsole(LocalFrame& frame)
    : m_frame(frame)
{
}

void FrameConsole::addMessage(MessageSource source, MessageLevel level, const String& message)
{
    addMessage(source, level, message, String(), 0, 0, nullptr, 0, 0);
}

void FrameConsole::addMessage(MessageSource source, MessageLevel level, const String& message, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    addMessage(source, level, message, String(), 0, 0, callStack, 0);
}

void FrameConsole::addMessage(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, ScriptState* scriptState, unsigned long requestIdentifier)
{
    if (muteCount)
        return;

    // FIXME: This should not need to reach for the main-frame.
    // Inspector code should just take the current frame and know how to walk itself.
    ExecutionContext* context = m_frame.document();
    if (!context)
        return;

    String messageURL;
    if (callStack) {
        messageURL = callStack->at(0).sourceURL();
        InspectorInstrumentation::addMessageToConsole(context, source, LogMessageType, level, message, callStack, requestIdentifier);
    } else {
        messageURL = url;
        InspectorInstrumentation::addMessageToConsole(context, source, LogMessageType, level, message, url, lineNumber, columnNumber, scriptState, requestIdentifier);
    }

    if (source == CSSMessageSource)
        return;

    String stackTrace;
    if (callStack && m_frame.chromeClient().shouldReportDetailedMessageForSource(messageURL))
        stackTrace = FrameConsole::formatStackTraceString(message, callStack);

    m_frame.chromeClient().addMessageToConsole(&m_frame, source, level, message, lineNumber, messageURL, stackTrace);
}

String FrameConsole::formatStackTraceString(const String& originalMessage, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    StringBuilder stackTrace;
    for (size_t i = 0; i < callStack->size(); ++i) {
        const ScriptCallFrame& frame = callStack->at(i);
        stackTrace.append("\n    at " + (frame.functionName().length() ? frame.functionName() : "(anonymous function)"));
        stackTrace.append(" (");
        stackTrace.append(frame.sourceURL());
        stackTrace.append(':');
        stackTrace.append(String::number(frame.lineNumber()));
        stackTrace.append(':');
        stackTrace.append(String::number(frame.columnNumber()));
        stackTrace.append(')');
    }

    return stackTrace.toString();
}

void FrameConsole::mute()
{
    muteCount++;
}

void FrameConsole::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

} // namespace WebCore
