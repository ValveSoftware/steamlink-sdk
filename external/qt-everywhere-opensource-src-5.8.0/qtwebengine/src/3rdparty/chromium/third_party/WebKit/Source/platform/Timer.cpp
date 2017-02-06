/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/Timer.h"

#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Atomics.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashSet.h"
#include <algorithm>
#include <limits.h>
#include <limits>
#include <math.h>

namespace blink {

TimerBase::TimerBase() : TimerBase(Platform::current()->currentThread()->scheduler()->timerTaskRunner()) { }

TimerBase::TimerBase(WebTaskRunner* webTaskRunner)
    : m_nextFireTime(0)
    , m_repeatInterval(0)
    , m_cancellableTimerTask(nullptr)
    , m_webTaskRunner(webTaskRunner)
#if DCHECK_IS_ON()
    , m_thread(currentThread())
#endif
{
    ASSERT(m_webTaskRunner);
}

TimerBase::~TimerBase()
{
    stop();
}

void TimerBase::start(double nextFireInterval, double repeatInterval, const WebTraceLocation& caller)
{
    ASSERT(m_thread == currentThread());

    m_location = caller;
    m_repeatInterval = repeatInterval;
    setNextFireTime(timerMonotonicallyIncreasingTime(), nextFireInterval);
}

void TimerBase::stop()
{
    ASSERT(m_thread == currentThread());

    m_repeatInterval = 0;
    m_nextFireTime = 0;
    if (m_cancellableTimerTask)
        m_cancellableTimerTask->cancel();
    m_cancellableTimerTask = nullptr;
}

double TimerBase::nextFireInterval() const
{
    ASSERT(isActive());
    double current = timerMonotonicallyIncreasingTime();
    if (m_nextFireTime < current)
        return 0;
    return m_nextFireTime - current;
}

WebTaskRunner* TimerBase::timerTaskRunner() const
{
    return m_webTaskRunner;
}

void TimerBase::setNextFireTime(double now, double delay)
{
    ASSERT(m_thread == currentThread());

    double newTime = now + delay;

    if (m_nextFireTime != newTime) {
        m_nextFireTime = newTime;
        if (m_cancellableTimerTask)
            m_cancellableTimerTask->cancel();
        m_cancellableTimerTask = new CancellableTimerTask(this);

        double delayMs = 1000.0 * (newTime - now);
        timerTaskRunner()->postDelayedTask(m_location, m_cancellableTimerTask, delayMs);
    }
}

NO_LAZY_SWEEP_SANITIZE_ADDRESS
void TimerBase::runInternal()
{
    if (!canFire())
        return;

    TRACE_EVENT0("blink", "TimerBase::run");
#if DCHECK_IS_ON()
    DCHECK_EQ(m_thread, currentThread()) << "Timer posted by " << m_location.functionName() << " " << m_location.fileName() << " was run on a different thread";
#endif
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "BlinkInternal");

    if (m_repeatInterval) {
        double now = timerMonotonicallyIncreasingTime();
        // This computation should be drift free, and it will cope if we miss a beat,
        // which can easily happen if the thread is busy.  It will also cope if we get
        // called slightly before m_unalignedNextFireTime, which can happen due to lack
        // of timer precision.
        double intervalToNextFireTime = m_repeatInterval - fmod(now - m_nextFireTime, m_repeatInterval);
        setNextFireTime(timerMonotonicallyIncreasingTime(), intervalToNextFireTime);
    } else {
        m_nextFireTime = 0;
    }
    fired();
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "Sleeping");
}

bool TimerBase::Comparator::operator()(const TimerBase* a, const TimerBase* b) const
{
    return a->m_nextFireTime < b->m_nextFireTime;
}

// static
WebTaskRunner* TimerBase::UnthrottledWebTaskRunner()
{
    return Platform::current()->currentThread()->getWebTaskRunner();
}

double TimerBase::timerMonotonicallyIncreasingTime() const
{
    return timerTaskRunner()->monotonicallyIncreasingVirtualTimeSeconds();
}

} // namespace blink
