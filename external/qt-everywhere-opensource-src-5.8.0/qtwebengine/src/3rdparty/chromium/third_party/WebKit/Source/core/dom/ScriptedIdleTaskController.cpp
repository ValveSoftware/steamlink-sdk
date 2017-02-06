// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptedIdleTaskController.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/IdleRequestCallback.h"
#include "core/dom/IdleRequestOptions.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/Histogram.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"
#include "wtf/RefCounted.h"

namespace blink {

namespace internal {

class IdleRequestCallbackWrapper : public RefCounted<IdleRequestCallbackWrapper> {
public:
    static PassRefPtr<IdleRequestCallbackWrapper> create(ScriptedIdleTaskController::CallbackId id, ScriptedIdleTaskController* controller)
    {
        return adoptRef(new IdleRequestCallbackWrapper(id, controller));
    }
    virtual ~IdleRequestCallbackWrapper()
    {
    }

    static void idleTaskFired(PassRefPtr<IdleRequestCallbackWrapper> callbackWrapper, double deadlineSeconds)
    {
        // TODO(rmcilroy): Implement clamping of deadline in some form.
        if (ScriptedIdleTaskController* controller = callbackWrapper->controller())
            controller->callbackFired(callbackWrapper->id(), deadlineSeconds, IdleDeadline::CallbackType::CalledWhenIdle);
        callbackWrapper->cancel();
    }

    static void timeoutFired(PassRefPtr<IdleRequestCallbackWrapper> callbackWrapper)
    {
        if (ScriptedIdleTaskController* controller = callbackWrapper->controller())
            controller->callbackFired(callbackWrapper->id(), monotonicallyIncreasingTime(), IdleDeadline::CallbackType::CalledByTimeout);
        callbackWrapper->cancel();
    }

    void cancel()
    {
        m_controller = nullptr;
    }

    ScriptedIdleTaskController::CallbackId id() const { return m_id; }
    ScriptedIdleTaskController* controller() const { return m_controller; }

private:
    IdleRequestCallbackWrapper(ScriptedIdleTaskController::CallbackId id, ScriptedIdleTaskController* controller)
        : m_id(id)
        , m_controller(controller)
    {
    }

    ScriptedIdleTaskController::CallbackId m_id;
    Persistent<ScriptedIdleTaskController> m_controller;
};

} // namespace internal

ScriptedIdleTaskController::ScriptedIdleTaskController(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_scheduler(Platform::current()->currentThread()->scheduler())
    , m_nextCallbackId(0)
    , m_suspended(false)
{
    suspendIfNeeded();
}

ScriptedIdleTaskController::~ScriptedIdleTaskController()
{
}

DEFINE_TRACE(ScriptedIdleTaskController)
{
    visitor->trace(m_callbacks);
    ActiveDOMObject::trace(visitor);
}

int ScriptedIdleTaskController::nextCallbackId()
{
    while (true) {
        ++m_nextCallbackId;

        if (!isValidCallbackId(m_nextCallbackId))
            m_nextCallbackId = 1;

        if (!m_callbacks.contains(m_nextCallbackId))
            return m_nextCallbackId;
    }
}

ScriptedIdleTaskController::CallbackId ScriptedIdleTaskController::registerCallback(IdleRequestCallback* callback, const IdleRequestOptions& options)
{
    CallbackId id = nextCallbackId();
    m_callbacks.set(id, callback);
    long long timeoutMillis = options.timeout();

    RefPtr<internal::IdleRequestCallbackWrapper> callbackWrapper = internal::IdleRequestCallbackWrapper::create(id, this);
    m_scheduler->postIdleTask(BLINK_FROM_HERE, WTF::bind(&internal::IdleRequestCallbackWrapper::idleTaskFired, callbackWrapper));
    if (timeoutMillis > 0)
        m_scheduler->timerTaskRunner()->postDelayedTask(BLINK_FROM_HERE, WTF::bind(&internal::IdleRequestCallbackWrapper::timeoutFired, callbackWrapper), timeoutMillis);
    TRACE_EVENT_INSTANT1("devtools.timeline", "RequestIdleCallback", TRACE_EVENT_SCOPE_THREAD, "data", InspectorIdleCallbackRequestEvent::data(getExecutionContext(), id, timeoutMillis));
    return id;
}

void ScriptedIdleTaskController::cancelCallback(CallbackId id)
{
    TRACE_EVENT_INSTANT1("devtools.timeline", "CancelIdleCallback", TRACE_EVENT_SCOPE_THREAD, "data", InspectorIdleCallbackCancelEvent::data(getExecutionContext(), id));
    if (!isValidCallbackId(id))
        return;

    m_callbacks.remove(id);
}

void ScriptedIdleTaskController::callbackFired(CallbackId id, double deadlineSeconds, IdleDeadline::CallbackType callbackType)
{
    if (!m_callbacks.contains(id))
        return;

    if (m_suspended) {
        if (callbackType == IdleDeadline::CallbackType::CalledByTimeout) {
            // Queue for execution when we are resumed.
            m_pendingTimeouts.append(id);
        }
        // Just drop callbacks called while suspended, these will be reposted on the idle task queue when we are resumed.
        return;
    }

    runCallback(id, deadlineSeconds, callbackType);
}

void ScriptedIdleTaskController::runCallback(CallbackId id, double deadlineSeconds, IdleDeadline::CallbackType callbackType)
{
    DCHECK(!m_suspended);
    auto callback = m_callbacks.take(id);
    if (!callback)
        return;

    double allottedTimeMillis = std::max((deadlineSeconds - monotonicallyIncreasingTime()) * 1000, 0.0);

    DEFINE_STATIC_LOCAL(CustomCountHistogram, idleCallbackDeadlineHistogram, ("WebCore.ScriptedIdleTaskController.IdleCallbackDeadline", 0, 50, 50));
    idleCallbackDeadlineHistogram.count(allottedTimeMillis);

    TRACE_EVENT1("devtools.timeline", "FireIdleCallback",
        "data", InspectorIdleCallbackFireEvent::data(getExecutionContext(), id, allottedTimeMillis, callbackType == IdleDeadline::CallbackType::CalledByTimeout));
    callback->handleEvent(IdleDeadline::create(deadlineSeconds, callbackType));

    double overrunMillis = std::max((monotonicallyIncreasingTime() - deadlineSeconds) * 1000, 0.0);

    DEFINE_STATIC_LOCAL(CustomCountHistogram, idleCallbackOverrunHistogram, ("WebCore.ScriptedIdleTaskController.IdleCallbackOverrun", 0, 10000, 50));
    idleCallbackOverrunHistogram.count(overrunMillis);
}

void ScriptedIdleTaskController::stop()
{
    m_callbacks.clear();
}

void ScriptedIdleTaskController::suspend()
{
    m_suspended = true;
}

void ScriptedIdleTaskController::resume()
{
    DCHECK(m_suspended);
    m_suspended = false;

    // Run any pending timeouts.
    Vector<CallbackId> pendingTimeouts;
    m_pendingTimeouts.swap(pendingTimeouts);
    for (auto& id : pendingTimeouts)
        runCallback(id, monotonicallyIncreasingTime(), IdleDeadline::CallbackType::CalledByTimeout);

    // Repost idle tasks for any remaining callbacks.
    for (auto& callback : m_callbacks) {
        RefPtr<internal::IdleRequestCallbackWrapper> callbackWrapper = internal::IdleRequestCallbackWrapper::create(callback.key, this);
        m_scheduler->postIdleTask(BLINK_FROM_HERE, WTF::bind(&internal::IdleRequestCallbackWrapper::idleTaskFired, callbackWrapper));
    }
}

} // namespace blink
