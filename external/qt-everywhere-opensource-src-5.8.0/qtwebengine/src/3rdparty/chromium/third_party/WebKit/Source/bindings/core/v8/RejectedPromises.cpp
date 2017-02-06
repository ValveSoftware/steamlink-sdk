// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/RejectedPromises.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "core/events/PromiseRejectionEvent.h"
#include "core/inspector/ThreadDebugger.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static const unsigned maxReportedHandlersPendingResolution = 1000;

class RejectedPromises::Message final {
public:
    static std::unique_ptr<Message> create(ScriptState* scriptState, v8::Local<v8::Promise> promise, v8::Local<v8::Value> exception, const String& errorMessage, std::unique_ptr<SourceLocation> location, AccessControlStatus corsStatus)
    {
        return wrapUnique(new Message(scriptState, promise, exception, errorMessage, std::move(location), corsStatus));
    }

    bool isCollected()
    {
        return m_collected || !m_scriptState->contextIsValid();
    }

    bool hasPromise(v8::Local<v8::Value> promise)
    {
        ScriptState::Scope scope(m_scriptState);
        return promise == m_promise.newLocal(m_scriptState->isolate());
    }

    void report()
    {
        if (!m_scriptState->contextIsValid())
            return;
        // If execution termination has been triggered, quietly bail out.
        if (m_scriptState->isolate()->IsExecutionTerminating())
            return;
        ExecutionContext* executionContext = m_scriptState->getExecutionContext();
        if (!executionContext)
            return;

        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        v8::Local<v8::Value> reason = m_exception.newLocal(m_scriptState->isolate());
        // Either collected or https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            return;
        ASSERT(!hasHandler());

        EventTarget* target = executionContext->errorEventTarget();
        if (target && !executionContext->shouldSanitizeScriptError(m_resourceName, m_corsStatus)) {
            PromiseRejectionEventInit init;
            init.setPromise(ScriptPromise(m_scriptState, value));
            init.setReason(ScriptValue(m_scriptState, reason));
            init.setCancelable(true);
            PromiseRejectionEvent* event = PromiseRejectionEvent::create(m_scriptState, EventTypeNames::unhandledrejection, init);
            // Log to console if event was not canceled.
            m_shouldLogToConsole = target->dispatchEvent(event) == DispatchEventResult::NotCanceled;
        }

        if (m_shouldLogToConsole) {
            V8PerIsolateData* data = V8PerIsolateData::from(m_scriptState->isolate());
            if (data->threadDebugger())
                m_promiseRejectionId = data->threadDebugger()->debugger()->promiseRejected(m_scriptState->context(), m_errorMessage, reason, m_location->url(), m_location->lineNumber(), m_location->columnNumber(), m_location->cloneStackTrace(), m_location->scriptId());
        }

        m_location.reset();
    }

    void revoke()
    {
        ExecutionContext* executionContext = m_scriptState->getExecutionContext();
        if (!executionContext)
            return;

        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        v8::Local<v8::Value> reason = m_exception.newLocal(m_scriptState->isolate());
        // Either collected or https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            return;

        EventTarget* target = executionContext->errorEventTarget();
        if (target && !executionContext->shouldSanitizeScriptError(m_resourceName, m_corsStatus)) {
            PromiseRejectionEventInit init;
            init.setPromise(ScriptPromise(m_scriptState, value));
            init.setReason(ScriptValue(m_scriptState, reason));
            PromiseRejectionEvent* event = PromiseRejectionEvent::create(m_scriptState, EventTypeNames::rejectionhandled, init);
            target->dispatchEvent(event);
        }

        if (m_shouldLogToConsole && m_promiseRejectionId) {
            V8PerIsolateData* data = V8PerIsolateData::from(m_scriptState->isolate());
            if (data->threadDebugger())
                data->threadDebugger()->debugger()->promiseRejectionRevoked(m_scriptState->context(), m_promiseRejectionId);
        }
    }

    void makePromiseWeak()
    {
        ASSERT(!m_promise.isEmpty() && !m_promise.isWeak());
        m_promise.setWeak(this, &Message::didCollectPromise);
        m_exception.setWeak(this, &Message::didCollectException);
    }

    void makePromiseStrong()
    {
        ASSERT(!m_promise.isEmpty() && m_promise.isWeak());
        m_promise.clearWeak();
        m_exception.clearWeak();
    }

    bool hasHandler()
    {
        ASSERT(!isCollected());
        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        return v8::Local<v8::Promise>::Cast(value)->HasHandler();
    }

private:
    Message(ScriptState* scriptState, v8::Local<v8::Promise> promise, v8::Local<v8::Value> exception, const String& errorMessage, std::unique_ptr<SourceLocation> location, AccessControlStatus corsStatus)
        : m_scriptState(scriptState)
        , m_promise(scriptState->isolate(), promise)
        , m_exception(scriptState->isolate(), exception)
        , m_errorMessage(errorMessage)
        , m_resourceName(location->url())
        , m_location(std::move(location))
        , m_promiseRejectionId(0)
        , m_collected(false)
        , m_shouldLogToConsole(true)
        , m_corsStatus(corsStatus)
    {
    }

    static void didCollectPromise(const v8::WeakCallbackInfo<Message>& data)
    {
        data.GetParameter()->m_collected = true;
        data.GetParameter()->m_promise.clear();
    }

    static void didCollectException(const v8::WeakCallbackInfo<Message>& data)
    {
        data.GetParameter()->m_exception.clear();
    }

    ScriptState* m_scriptState;
    ScopedPersistent<v8::Promise> m_promise;
    ScopedPersistent<v8::Value> m_exception;
    String m_errorMessage;
    String m_resourceName;
    std::unique_ptr<SourceLocation> m_location;
    unsigned m_promiseRejectionId;
    bool m_collected;
    bool m_shouldLogToConsole;
    AccessControlStatus m_corsStatus;
};

RejectedPromises::RejectedPromises()
{
}

RejectedPromises::~RejectedPromises()
{
}

void RejectedPromises::rejectedWithNoHandler(ScriptState* scriptState, v8::PromiseRejectMessage data, const String& errorMessage, std::unique_ptr<SourceLocation> location, AccessControlStatus corsStatus)
{
    m_queue.append(Message::create(scriptState, data.GetPromise(), data.GetValue(), errorMessage, std::move(location), corsStatus));
}

void RejectedPromises::handlerAdded(v8::PromiseRejectMessage data)
{
    // First look it up in the pending messages and fast return, it'll be covered by processQueue().
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (!(*it)->isCollected() && (*it)->hasPromise(data.GetPromise())) {
            m_queue.remove(it);
            return;
        }
    }

    // Then look it up in the reported errors.
    for (size_t i = 0; i < m_reportedAsErrors.size(); ++i) {
        std::unique_ptr<Message>& message = m_reportedAsErrors.at(i);
        if (!message->isCollected() && message->hasPromise(data.GetPromise())) {
            message->makePromiseStrong();
            Platform::current()->currentThread()->scheduler()->timerTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&RejectedPromises::revokeNow, RefPtr<RejectedPromises>(this), passed(std::move(message))));
            m_reportedAsErrors.remove(i);
            return;
        }
    }
}

std::unique_ptr<RejectedPromises::MessageQueue> RejectedPromises::createMessageQueue()
{
    return wrapUnique(new MessageQueue());
}

void RejectedPromises::dispose()
{
    if (m_queue.isEmpty())
        return;

    std::unique_ptr<MessageQueue> queue = createMessageQueue();
    queue->swap(m_queue);
    processQueueNow(std::move(queue));
}

void RejectedPromises::processQueue()
{
    if (m_queue.isEmpty())
        return;

    std::unique_ptr<MessageQueue> queue = createMessageQueue();
    queue->swap(m_queue);
    Platform::current()->currentThread()->scheduler()->timerTaskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&RejectedPromises::processQueueNow, PassRefPtr<RejectedPromises>(this), passed(std::move(queue))));
}

void RejectedPromises::processQueueNow(std::unique_ptr<MessageQueue> queue)
{
    // Remove collected handlers.
    for (size_t i = 0; i < m_reportedAsErrors.size();) {
        if (m_reportedAsErrors.at(i)->isCollected())
            m_reportedAsErrors.remove(i);
        else
            ++i;
    }

    while (!queue->isEmpty()) {
        std::unique_ptr<Message> message = queue->takeFirst();
        if (message->isCollected())
            continue;
        if (!message->hasHandler()) {
            message->report();
            message->makePromiseWeak();
            m_reportedAsErrors.append(std::move(message));
            if (m_reportedAsErrors.size() > maxReportedHandlersPendingResolution)
                m_reportedAsErrors.remove(0, maxReportedHandlersPendingResolution / 10);
        }
    }
}

void RejectedPromises::revokeNow(std::unique_ptr<Message> message)
{
    message->revoke();
}

} // namespace blink
