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

#include "platform/tracing/TraceEvent.h"
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

TimerBase::TimerBase(WebTaskRunner* webTaskRunner)
    : m_nextFireTime(0),
      m_repeatInterval(0),
      m_webTaskRunner(webTaskRunner->clone()),
#if DCHECK_IS_ON()
      m_thread(currentThread()),
#endif
      m_weakPtrFactory(this) {
  ASSERT(m_webTaskRunner);
}

TimerBase::~TimerBase() {
  stop();
}

void TimerBase::start(double nextFireInterval,
                      double repeatInterval,
                      const WebTraceLocation& caller) {
  ASSERT(m_thread == currentThread());

  m_location = caller;
  m_repeatInterval = repeatInterval;
  setNextFireTime(timerMonotonicallyIncreasingTime(), nextFireInterval);
}

void TimerBase::stop() {
  ASSERT(m_thread == currentThread());

  m_repeatInterval = 0;
  m_nextFireTime = 0;
  m_weakPtrFactory.revokeAll();
}

double TimerBase::nextFireInterval() const {
  ASSERT(isActive());
  double current = timerMonotonicallyIncreasingTime();
  if (m_nextFireTime < current)
    return 0;
  return m_nextFireTime - current;
}

// static
WebTaskRunner* TimerBase::getTimerTaskRunner() {
  return Platform::current()->currentThread()->scheduler()->timerTaskRunner();
}

// static
WebTaskRunner* TimerBase::getUnthrottledTaskRunner() {
  return Platform::current()->currentThread()->getWebTaskRunner();
}

WebTaskRunner* TimerBase::timerTaskRunner() const {
  return m_webTaskRunner.get();
}

void TimerBase::setNextFireTime(double now, double delay) {
  ASSERT(m_thread == currentThread());

  double newTime = now + delay;

  if (m_nextFireTime != newTime) {
    m_nextFireTime = newTime;

    // Cancel any previously posted task.
    m_weakPtrFactory.revokeAll();

    double delayMs = 1000.0 * (newTime - now);
    timerTaskRunner()->postDelayedTask(
        m_location,
        base::Bind(&TimerBase::runInternal, m_weakPtrFactory.createWeakPtr()),
        delayMs);
  }
}

NO_SANITIZE_ADDRESS
void TimerBase::runInternal() {
  if (!canFire())
    return;

  m_weakPtrFactory.revokeAll();

  TRACE_EVENT0("blink", "TimerBase::run");
#if DCHECK_IS_ON()
  DCHECK_EQ(m_thread, currentThread())
      << "Timer posted by " << m_location.function_name() << " "
      << m_location.file_name() << " was run on a different thread";
#endif

  if (m_repeatInterval) {
    double now = timerMonotonicallyIncreasingTime();
    // This computation should be drift free, and it will cope if we miss a
    // beat, which can easily happen if the thread is busy.  It will also cope
    // if we get called slightly before m_unalignedNextFireTime, which can
    // happen due to lack of timer precision.
    double intervalToNextFireTime =
        m_repeatInterval - fmod(now - m_nextFireTime, m_repeatInterval);
    setNextFireTime(timerMonotonicallyIncreasingTime(), intervalToNextFireTime);
  } else {
    m_nextFireTime = 0;
  }
  fired();
}

bool TimerBase::Comparator::operator()(const TimerBase* a,
                                       const TimerBase* b) const {
  return a->m_nextFireTime < b->m_nextFireTime;
}

// static
double TimerBase::timerMonotonicallyIncreasingTime() const {
  return timerTaskRunner()->monotonicallyIncreasingVirtualTimeSeconds();
}

}  // namespace blink
