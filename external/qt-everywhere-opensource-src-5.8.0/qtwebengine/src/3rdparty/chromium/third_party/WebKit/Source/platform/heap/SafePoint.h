// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SafePoint_h
#define SafePoint_h

#include "platform/heap/ThreadState.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

class SafePointScope final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(SafePointScope);
public:
    explicit SafePointScope(BlinkGC::StackState stackState, ThreadState* state = ThreadState::current())
        : m_state(state)
    {
        if (m_state) {
            RELEASE_ASSERT(!m_state->isAtSafePoint());
            m_state->enterSafePoint(stackState, this);
        }
    }

    ~SafePointScope()
    {
        if (m_state)
            m_state->leaveSafePoint();
    }

private:
    ThreadState* m_state;
};

// The SafePointAwareMutexLocker is used to enter a safepoint while waiting for
// a mutex lock. It also ensures that the lock is not held while waiting for a GC
// to complete in the leaveSafePoint method, by releasing the lock if the
// leaveSafePoint method cannot complete without blocking, see
// SafePointBarrier::checkAndPark.
class SafePointAwareMutexLocker final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(SafePointAwareMutexLocker);
public:
    explicit SafePointAwareMutexLocker(MutexBase& mutex, BlinkGC::StackState stackState = BlinkGC::HeapPointersOnStack)
        : m_mutex(mutex)
        , m_locked(false)
    {
        ThreadState* state = ThreadState::current();
        do {
            bool leaveSafePoint = false;
            // We cannot enter a safepoint if we are currently sweeping. In that
            // case we just try to acquire the lock without being at a safepoint.
            // If another thread tries to do a GC at that time it might time out
            // due to this thread not being at a safepoint and waiting on the lock.
            if (!state->sweepForbidden() && !state->isAtSafePoint()) {
                state->enterSafePoint(stackState, this);
                leaveSafePoint = true;
            }
            m_mutex.lock();
            m_locked = true;
            if (leaveSafePoint) {
                // When leaving the safepoint we might end up release the mutex
                // if another thread is requesting a GC, see
                // SafePointBarrier::checkAndPark. This is the case where we
                // loop around to reacquire the lock.
                state->leaveSafePoint(this);
            }
        } while (!m_locked);
    }

    ~SafePointAwareMutexLocker()
    {
        ASSERT(m_locked);
        m_mutex.unlock();
    }

private:
    friend class SafePointBarrier;

    void reset()
    {
        ASSERT(m_locked);
        m_mutex.unlock();
        m_locked = false;
    }

    MutexBase& m_mutex;
    bool m_locked;
};

class SafePointBarrier final {
    USING_FAST_MALLOC(SafePointBarrier);
    WTF_MAKE_NONCOPYABLE(SafePointBarrier);
public:
    SafePointBarrier();
    ~SafePointBarrier();

    // Request other attached threads that are not at safe points to park
    // themselves on safepoints.
    bool parkOthers();

    // Resume executions of other attached threads that are parked at
    // the safe points.
    void resumeOthers(bool barrierLocked = false);

    // Park this thread if there exists a request to park attached threads.
    // This method must be called at a safe point.
    void checkAndPark(ThreadState*, SafePointAwareMutexLocker* = nullptr);

    void enterSafePoint(ThreadState*);
    void leaveSafePoint(ThreadState*, SafePointAwareMutexLocker* = nullptr);

private:
    void doPark(ThreadState*, intptr_t* stackEnd);
    static void parkAfterPushRegisters(SafePointBarrier* barrier, ThreadState* state, intptr_t* stackEnd)
    {
        barrier->doPark(state, stackEnd);
    }
    void doEnterSafePoint(ThreadState*, intptr_t* stackEnd);
    static void enterSafePointAfterPushRegisters(SafePointBarrier* barrier, ThreadState* state, intptr_t* stackEnd)
    {
        barrier->doEnterSafePoint(state, stackEnd);
    }

    // |m_unparkedThreadCount| tracks amount of unparked threads. It is
    // positive if and only if a thread has requested the other threads
    // to park themselves at safe-points in preparation for a GC.
    //
    // The last thread to park itself will make the counter hit zero
    // and should notify GC-requesting thread that it is safe to proceed.
    //
    // If no other thread is waiting for other threads to park then
    // this counter can be negative: if N threads are at safe-points
    // the counter will be -N.
    volatile int m_unparkedThreadCount;

    // |m_parkingRequested| is used to control the transition of threads parked
    // at a safepoint back to running state. In the event a thread requests
    // another GC, threads that have yet to leave their safepoint (due to lock
    // contention, scheduling etc), shouldn't be allowed to leave, but continue
    // being parked when they do end up getting to run.
    //
    // |m_parkingRequested| is set when parkOthers() runs, and cleared by
    // resumeOthers(), when the global GC steps have completed.
    //
    // Threads that were parked after they were requested to and then signalled,
    // check that no other thread has made another parking request when attempting
    // to resume in doPark().
    volatile int m_parkingRequested;

    Mutex m_mutex;

    ThreadCondition m_parked;
    ThreadCondition m_resume;
};

} // namespace blink

#endif
