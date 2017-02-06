/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 *
 */
#ifndef LifecycleNotifier_h
#define LifecycleNotifier_h

#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/TemporaryChange.h"

namespace blink {

template<typename T, typename Observer>
class LifecycleNotifier : public virtual GarbageCollectedMixin {
public:
    virtual ~LifecycleNotifier();

    void addObserver(Observer*);
    void removeObserver(Observer*);

    // notifyContextDestroyed() should be explicitly dispatched from an
    // observed context to notify observers that contextDestroyed().
    //
    // When contextDestroyed() is called, the observer's lifecycleContext()
    // is still valid and safe to use during the notification.
    virtual void notifyContextDestroyed();

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_observers);
    }

    bool isIteratingOverObservers() const { return m_iterationState != NotIterating; }

protected:
    LifecycleNotifier()
        : m_iterationState(NotIterating)
    {
    }

#if DCHECK_IS_ON()
    T* context() { return static_cast<T*>(this); }
#endif

    using ObserverSet = HeapHashSet<WeakMember<Observer>>;

    void removePending(ObserverSet&);

    enum IterationState {
        AllowingNone        = 0,
        AllowingAddition    = 1,
        AllowingRemoval     = 2,
        NotIterating        = AllowingAddition | AllowingRemoval,
        AllowPendingRemoval = 4,
    };

    // Iteration state is recorded while iterating the observer set,
    // optionally barring add or remove mutations.
    IterationState m_iterationState;
    ObserverSet m_observers;
};

template<typename T, typename Observer>
inline LifecycleNotifier<T, Observer>::~LifecycleNotifier()
{
    // FIXME: Enable the following ASSERT. Also see a FIXME in Document::detach().
    // ASSERT(!m_observers.size());
}

template<typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::notifyContextDestroyed()
{
    // Observer unregistration is allowed, but effectively a no-op.
    TemporaryChange<IterationState> scope(m_iterationState, AllowingRemoval);
    ObserverSet observers;
    m_observers.swap(observers);
    for (Observer* observer : observers) {
        ASSERT(observer->lifecycleContext() == context());
        observer->contextDestroyed();
    }
}

template<typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::addObserver(Observer* observer)
{
    RELEASE_ASSERT(m_iterationState & AllowingAddition);
    m_observers.add(observer);
}

template<typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::removeObserver(Observer* observer)
{
    // If immediate removal isn't currently allowed,
    // |observer| is recorded for pending removal.
    if (m_iterationState & AllowPendingRemoval) {
        m_observers.add(observer);
        return;
    }
    RELEASE_ASSERT(m_iterationState & AllowingRemoval);
    m_observers.remove(observer);
}

template<typename T, typename Observer>
inline void LifecycleNotifier<T, Observer>::removePending(ObserverSet& observers)
{
    if (m_observers.size()) {
        // Prevent allocation (==shrinking) while removing;
        // the table is likely to become garbage soon.
        ThreadState::NoAllocationScope scope(ThreadState::current());
        observers.removeAll(m_observers);
    }
    m_observers.swap(observers);
}

} // namespace blink

#endif // LifecycleNotifier_h
