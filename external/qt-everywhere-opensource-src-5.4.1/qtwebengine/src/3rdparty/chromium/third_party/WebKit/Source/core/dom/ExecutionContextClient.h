/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExecutionContextClient_h
#define ExecutionContextClient_h

#include "core/frame/ConsoleTypes.h"
#include "platform/LifecycleNotifier.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"

namespace WebCore {

class LocalDOMWindow;
class EventQueue;
class EventTarget;
class ExecutionContextTask;
class KURL;
template<class T> class LifecycleNotifier;
class ScriptCallStack;
class ScriptState;
class SecurityContext;

class ExecutionContextClient {
public:
    virtual void postTask(PassOwnPtr<ExecutionContextTask>) = 0; // Executes the task on context's thread asynchronously.

    virtual bool isDocument() const { return false; }
    virtual bool isWorkerGlobalScope() const { return false; }
    virtual bool isJSExecutionForbidden() const = 0;
    virtual LocalDOMWindow* executingWindow() { return 0; }
    virtual String userAgent(const KURL&) const = 0;
    virtual void disableEval(const String& errorMessage) = 0;
    virtual SecurityContext& securityContext() = 0;
    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState*) = 0;
    virtual EventTarget* errorEventTarget() = 0;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>) = 0;
    virtual double timerAlignmentInterval() const = 0;
    virtual void didUpdateSecurityOrigin() = 0;

    virtual void tasksWereSuspended() { }
    virtual void tasksWereResumed() { }

    void addConsoleMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber) { addMessage(source, level, message, sourceURL, lineNumber, 0); }
    void addConsoleMessage(MessageSource source, MessageLevel level, const String& message, ScriptState* state = 0) { addMessage(source, level, message, String(), 0, state); }

protected:
    virtual ~ExecutionContextClient() { }

};


} // namespace

#endif
