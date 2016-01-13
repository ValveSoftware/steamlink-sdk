/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/inspector/AsyncCallStackTracker.h"

#include "bindings/v8/V8RecursionScope.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "core/xml/XMLHttpRequest.h"
#include "core/xml/XMLHttpRequestUpload.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringBuilder.h"
#include <v8.h>

namespace {

static const char setTimeoutName[] = "setTimeout";
static const char setIntervalName[] = "setInterval";
static const char requestAnimationFrameName[] = "requestAnimationFrame";
static const char xhrSendName[] = "XMLHttpRequest.send";
static const char enqueueMutationRecordName[] = "Mutation";

}

namespace WebCore {

class AsyncCallStackTracker::ExecutionContextData FINAL : public ContextLifecycleObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ExecutionContextData(AsyncCallStackTracker* tracker, ExecutionContext* executionContext)
        : ContextLifecycleObserver(executionContext)
        , m_tracker(tracker)
    {
    }

    virtual void contextDestroyed() OVERRIDE
    {
        ASSERT(executionContext());
        ExecutionContextData* self = m_tracker->m_executionContextDataMap.take(executionContext());
        ASSERT(self == this);
        ContextLifecycleObserver::contextDestroyed();
        delete self;
    }

public:
    AsyncCallStackTracker* m_tracker;
    HashSet<int> m_intervalTimerIds;
    HashMap<int, RefPtr<AsyncCallChain> > m_timerCallChains;
    HashMap<int, RefPtr<AsyncCallChain> > m_animationFrameCallChains;
    HashMap<Event*, RefPtr<AsyncCallChain> > m_eventCallChains;
    HashMap<EventTarget*, RefPtr<AsyncCallChain> > m_xhrCallChains;
    HashMap<MutationObserver*, RefPtr<AsyncCallChain> > m_mutationObserverCallChains;
};

static XMLHttpRequest* toXmlHttpRequest(EventTarget* eventTarget)
{
    const AtomicString& interfaceName = eventTarget->interfaceName();
    if (interfaceName == EventTargetNames::XMLHttpRequest)
        return static_cast<XMLHttpRequest*>(eventTarget);
    if (interfaceName == EventTargetNames::XMLHttpRequestUpload)
        return static_cast<XMLHttpRequestUpload*>(eventTarget)->xmlHttpRequest();
    return 0;
}

AsyncCallStackTracker::AsyncCallStack::AsyncCallStack(const String& description, const ScriptValue& callFrames)
    : m_description(description)
    , m_callFrames(callFrames)
{
}

AsyncCallStackTracker::AsyncCallStack::~AsyncCallStack()
{
}

AsyncCallStackTracker::AsyncCallStackTracker()
    : m_maxAsyncCallStackDepth(0)
{
}

void AsyncCallStackTracker::setAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        clear();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
}

const AsyncCallStackTracker::AsyncCallChain* AsyncCallStackTracker::currentAsyncCallChain() const
{
    if (m_currentAsyncCallChain)
        ensureMaxAsyncCallChainDepth(m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    return m_currentAsyncCallChain.get();
}

void AsyncCallStackTracker::didInstallTimer(ExecutionContext* context, int timerId, bool singleShot, const ScriptValue& callFrames)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(timerId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_timerCallChains.set(timerId, createAsyncCallChain(singleShot ? setTimeoutName : setIntervalName, callFrames));
    if (!singleShot)
        data->m_intervalTimerIds.add(timerId);
}

void AsyncCallStackTracker::didRemoveTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (timerId <= 0)
        return;
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data)
        return;
    data->m_intervalTimerIds.remove(timerId);
    data->m_timerCallChains.remove(timerId);
}

void AsyncCallStackTracker::willFireTimer(ExecutionContext* context, int timerId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(timerId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (ExecutionContextData* data = m_executionContextDataMap.get(context)) {
        if (data->m_intervalTimerIds.contains(timerId))
            setCurrentAsyncCallChain(data->m_timerCallChains.get(timerId));
        else
            setCurrentAsyncCallChain(data->m_timerCallChains.take(timerId));
    } else {
        setCurrentAsyncCallChain(nullptr);
    }
}

void AsyncCallStackTracker::didRequestAnimationFrame(ExecutionContext* context, int callbackId, const ScriptValue& callFrames)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(callbackId > 0);
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_animationFrameCallChains.set(callbackId, createAsyncCallChain(requestAnimationFrameName, callFrames));
}

void AsyncCallStackTracker::didCancelAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (callbackId <= 0)
        return;
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_animationFrameCallChains.remove(callbackId);
}

void AsyncCallStackTracker::willFireAnimationFrame(ExecutionContext* context, int callbackId)
{
    ASSERT(context);
    ASSERT(isEnabled());
    ASSERT(callbackId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(data->m_animationFrameCallChains.take(callbackId));
    else
        setCurrentAsyncCallChain(nullptr);
}

void AsyncCallStackTracker::didEnqueueEvent(EventTarget* eventTarget, Event* event, const ScriptValue& callFrames)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(eventTarget->executionContext());
    data->m_eventCallChains.set(event, createAsyncCallChain(event->type(), callFrames));
}

void AsyncCallStackTracker::didRemoveEvent(EventTarget* eventTarget, Event* event)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
        data->m_eventCallChains.remove(event);
}

void AsyncCallStackTracker::willHandleEvent(EventTarget* eventTarget, Event* event, EventListener* listener, bool useCapture)
{
    ASSERT(eventTarget->executionContext());
    ASSERT(isEnabled());
    if (XMLHttpRequest* xhr = toXmlHttpRequest(eventTarget)) {
        willHandleXHREvent(xhr, eventTarget, event);
    } else {
        if (ExecutionContextData* data = m_executionContextDataMap.get(eventTarget->executionContext()))
            setCurrentAsyncCallChain(data->m_eventCallChains.get(event));
        else
            setCurrentAsyncCallChain(nullptr);
    }
}

void AsyncCallStackTracker::willLoadXHR(XMLHttpRequest* xhr, const ScriptValue& callFrames)
{
    ASSERT(xhr->executionContext());
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(xhr->executionContext());
    data->m_xhrCallChains.set(xhr, createAsyncCallChain(xhrSendName, callFrames));
}

void AsyncCallStackTracker::willHandleXHREvent(XMLHttpRequest* xhr, EventTarget* eventTarget, Event* event)
{
    ASSERT(xhr->executionContext());
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(xhr->executionContext())) {
        bool isXHRDownload = (xhr == eventTarget);
        if (isXHRDownload && event->type() == EventTypeNames::loadend)
            setCurrentAsyncCallChain(data->m_xhrCallChains.take(xhr));
        else
            setCurrentAsyncCallChain(data->m_xhrCallChains.get(xhr));
    } else {
        setCurrentAsyncCallChain(nullptr);
    }
}

void AsyncCallStackTracker::didEnqueueMutationRecord(ExecutionContext* context, MutationObserver* observer, const ScriptValue& callFrames)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ExecutionContextData* data = createContextDataIfNeeded(context);
    data->m_mutationObserverCallChains.set(observer, createAsyncCallChain(enqueueMutationRecordName, callFrames));
}

bool AsyncCallStackTracker::hasEnqueuedMutationRecord(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        return data->m_mutationObserverCallChains.contains(observer);
    return false;
}

void AsyncCallStackTracker::didClearAllMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        data->m_mutationObserverCallChains.remove(observer);
}

void AsyncCallStackTracker::willDeliverMutationRecords(ExecutionContext* context, MutationObserver* observer)
{
    ASSERT(context);
    ASSERT(isEnabled());
    if (ExecutionContextData* data = m_executionContextDataMap.get(context))
        setCurrentAsyncCallChain(data->m_mutationObserverCallChains.take(observer));
    else
        setCurrentAsyncCallChain(nullptr);
}

void AsyncCallStackTracker::didFireAsyncCall()
{
    clearCurrentAsyncCallChain();
}

PassRefPtr<AsyncCallStackTracker::AsyncCallChain> AsyncCallStackTracker::createAsyncCallChain(const String& description, const ScriptValue& callFrames)
{
    RefPtr<AsyncCallChain> chain = adoptRef(m_currentAsyncCallChain ? new AsyncCallStackTracker::AsyncCallChain(*m_currentAsyncCallChain) : new AsyncCallStackTracker::AsyncCallChain());
    ensureMaxAsyncCallChainDepth(chain.get(), m_maxAsyncCallStackDepth - 1);
    chain->m_callStacks.prepend(adoptRef(new AsyncCallStackTracker::AsyncCallStack(description, callFrames)));
    return chain.release();
}

void AsyncCallStackTracker::setCurrentAsyncCallChain(PassRefPtr<AsyncCallChain> chain)
{
    if (V8RecursionScope::recursionLevel(v8::Isolate::GetCurrent())) {
        if (m_currentAsyncCallChain)
            ++m_nestedAsyncCallCount;
    } else {
        // Current AsyncCallChain corresponds to the bottommost JS call frame.
        m_currentAsyncCallChain = chain;
        m_nestedAsyncCallCount = m_currentAsyncCallChain ? 1 : 0;
    }
}

void AsyncCallStackTracker::clearCurrentAsyncCallChain()
{
    if (!m_nestedAsyncCallCount)
        return;
    --m_nestedAsyncCallCount;
    if (!m_nestedAsyncCallCount)
        m_currentAsyncCallChain.clear();
}

void AsyncCallStackTracker::ensureMaxAsyncCallChainDepth(AsyncCallChain* chain, unsigned maxDepth)
{
    while (chain->m_callStacks.size() > maxDepth)
        chain->m_callStacks.removeLast();
}

bool AsyncCallStackTracker::validateCallFrames(const ScriptValue& callFrames)
{
    return !callFrames.isEmpty();
}

AsyncCallStackTracker::ExecutionContextData* AsyncCallStackTracker::createContextDataIfNeeded(ExecutionContext* context)
{
    ExecutionContextData* data = m_executionContextDataMap.get(context);
    if (!data) {
        data = new AsyncCallStackTracker::ExecutionContextData(this, context);
        m_executionContextDataMap.set(context, data);
    }
    return data;
}

void AsyncCallStackTracker::clear()
{
    m_currentAsyncCallChain.clear();
    m_nestedAsyncCallCount = 0;
    ExecutionContextDataMap copy;
    m_executionContextDataMap.swap(copy);
    for (ExecutionContextDataMap::const_iterator it = copy.begin(); it != copy.end(); ++it)
        delete it->value;
}

} // namespace WebCore
