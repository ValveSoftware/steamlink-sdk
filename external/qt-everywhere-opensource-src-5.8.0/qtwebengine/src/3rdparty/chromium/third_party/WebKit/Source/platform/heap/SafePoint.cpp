// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/SafePoint.h"

#include "platform/heap/Heap.h"
#include "wtf/Atomics.h"
#include "wtf/CurrentTime.h"

namespace blink {

using PushAllRegistersCallback = void (*)(SafePointBarrier*, ThreadState*, intptr_t*);
extern "C" void pushAllRegisters(SafePointBarrier*, ThreadState*, PushAllRegistersCallback);

static double lockingTimeout()
{
    // Wait time for parking all threads is at most 100 ms.
    return 0.100;
}

SafePointBarrier::SafePointBarrier()
    : m_unparkedThreadCount(0)
    , m_parkingRequested(0)
{
}

SafePointBarrier::~SafePointBarrier()
{
}

bool SafePointBarrier::parkOthers()
{
    ASSERT(ThreadState::current()->isAtSafePoint());

    ThreadState* current = ThreadState::current();
    // Lock threadAttachMutex() to prevent threads from attaching.
    current->lockThreadAttachMutex();
    const ThreadStateSet& threads = current->heap().threads();

    MutexLocker locker(m_mutex);
    atomicAdd(&m_unparkedThreadCount, threads.size());
    releaseStore(&m_parkingRequested, 1);

    for (ThreadState* state : threads) {
        if (state == current)
            continue;

        for (auto& interruptor : state->interruptors())
            interruptor->requestInterrupt();
    }

    while (acquireLoad(&m_unparkedThreadCount) > 0) {
        double expirationTime = currentTime() + lockingTimeout();
        if (!m_parked.timedWait(m_mutex, expirationTime)) {
            // One of the other threads did not return to a safepoint within the maximum
            // time we allow for threads to be parked. Abandon the GC and resume the
            // currently parked threads.
            resumeOthers(true);
            return false;
        }
    }
    return true;
}

void SafePointBarrier::resumeOthers(bool barrierLocked)
{
    ThreadState* current = ThreadState::current();
    const ThreadStateSet& threads = current->heap().threads();
    atomicSubtract(&m_unparkedThreadCount, threads.size());
    releaseStore(&m_parkingRequested, 0);

    if (UNLIKELY(barrierLocked)) {
        m_resume.broadcast();
    } else {
        // FIXME: Resumed threads will all contend for m_mutex just
        // to unlock it later which is a waste of resources.
        MutexLocker locker(m_mutex);
        m_resume.broadcast();
    }

    current->unlockThreadAttachMutex();
    ASSERT(ThreadState::current()->isAtSafePoint());
}

void SafePointBarrier::checkAndPark(ThreadState* state, SafePointAwareMutexLocker* locker)
{
    ASSERT(!state->sweepForbidden());
    if (acquireLoad(&m_parkingRequested)) {
        // If we are leaving the safepoint from a SafePointAwareMutexLocker
        // call out to release the lock before going to sleep. This enables the
        // lock to be acquired in the sweep phase, e.g. during weak processing
        // or finalization. The SafePointAwareLocker will reenter the safepoint
        // and reacquire the lock after leaving this safepoint.
        if (locker)
            locker->reset();
        pushAllRegisters(this, state, parkAfterPushRegisters);
    }
}

void SafePointBarrier::enterSafePoint(ThreadState* state)
{
    ASSERT(!state->sweepForbidden());
    pushAllRegisters(this, state, enterSafePointAfterPushRegisters);
}

void SafePointBarrier::leaveSafePoint(ThreadState* state, SafePointAwareMutexLocker* locker)
{
    if (atomicIncrement(&m_unparkedThreadCount) > 0)
        checkAndPark(state, locker);
}

void SafePointBarrier::doPark(ThreadState* state, intptr_t* stackEnd)
{
    state->recordStackEnd(stackEnd);
    MutexLocker locker(m_mutex);
    if (!atomicDecrement(&m_unparkedThreadCount))
        m_parked.signal();
    while (acquireLoad(&m_parkingRequested))
        m_resume.wait(m_mutex);
    atomicIncrement(&m_unparkedThreadCount);
}

void SafePointBarrier::doEnterSafePoint(ThreadState* state, intptr_t* stackEnd)
{
    state->recordStackEnd(stackEnd);
    state->copyStackUntilSafePointScope();
    if (!atomicDecrement(&m_unparkedThreadCount)) {
        MutexLocker locker(m_mutex);
        m_parked.signal(); // Safe point reached.
    }
}

} // namespace blink
