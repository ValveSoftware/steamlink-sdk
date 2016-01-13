/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "config.h"

#include "core/workers/SharedWorkerGlobalScope.h"

#include "core/events/MessageEvent.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<MessageEvent> createConnectEvent(PassRefPtrWillBeRawPtr<MessagePort> prpPort)
{
    RefPtrWillBeRawPtr<MessagePort> port = prpPort;
    RefPtrWillBeRawPtr<MessageEvent> event = MessageEvent::create(adoptPtr(new MessagePortArray(1, port)), String(), String(), port);
    event->initEvent(EventTypeNames::connect, false, false);
    return event.release();
}

// static
PassRefPtrWillBeRawPtr<SharedWorkerGlobalScope> SharedWorkerGlobalScope::create(const String& name, SharedWorkerThread* thread, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    RefPtrWillBeRawPtr<SharedWorkerGlobalScope> context = adoptRefWillBeRefCountedGarbageCollected(new SharedWorkerGlobalScope(name, startupData->m_scriptURL, startupData->m_userAgent, thread, startupData->m_workerClients.release()));
    context->applyContentSecurityPolicyFromString(startupData->m_contentSecurityPolicy, startupData->m_contentSecurityPolicyType);
    return context.release();
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(const String& name, const KURL& url, const String& userAgent, SharedWorkerThread* thread, PassOwnPtrWillBeRawPtr<WorkerClients> workerClients)
    : WorkerGlobalScope(url, userAgent, thread, monotonicallyIncreasingTime(), workerClients)
    , m_name(name)
{
    ScriptWrappable::init(this);
}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope()
{
}

const AtomicString& SharedWorkerGlobalScope::interfaceName() const
{
    return EventTargetNames::SharedWorkerGlobalScope;
}

SharedWorkerThread* SharedWorkerGlobalScope::thread()
{
    return static_cast<SharedWorkerThread*>(Base::thread());
}

void SharedWorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    WorkerGlobalScope::logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, callStack);
    addMessageToWorkerConsole(JSMessageSource, ErrorMessageLevel, errorMessage, sourceURL, lineNumber, callStack, 0);
}

void SharedWorkerGlobalScope::trace(Visitor* visitor)
{
    WorkerGlobalScope::trace(visitor);
}

} // namespace WebCore
