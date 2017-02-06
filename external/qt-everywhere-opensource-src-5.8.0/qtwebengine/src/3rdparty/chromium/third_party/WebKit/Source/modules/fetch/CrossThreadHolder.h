// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CrossThreadHolder_h
#define CrossThreadHolder_h

#include "core/dom/ActiveDOMObject.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExecutionContext.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Locker.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadingPrimitives.h"
#include <memory>

namespace blink {

// CrossThreadHolder<T> provides cross-thread access to |obj| of class T
// bound to the thread of |executionContext| where |obj| is created.
// - CrossThreadHolder<T> can be passed across threads.
// - |obj|'s methods are called on the thread of |executionContext|
//   via CrossThreadHolder<T>::postTask().
// - |obj| is destructed on the thread of |executionContext|
//   when |executionContext| is stopped or
//   CrossThreadHolder is destructed (earlier of them).
//   Note: |obj|'s destruction can be slightly after CrossThreadHolder.
template<typename T>
class CrossThreadHolder final {
public:
    // Must be called on the thread where |obj| is created
    // (== the thread of |executionContext|).
    // The current thread must be attached to Oilpan.
    static std::unique_ptr<CrossThreadHolder<T>> create(ExecutionContext* executionContext, std::unique_ptr<T> obj)
    {
        ASSERT(executionContext->isContextThread());
        return wrapUnique(new CrossThreadHolder(executionContext, std::move(obj)));
    }

    // Can be called from any thread.
    // Executes |task| with |obj| and |executionContext| on the thread of
    // |executionContext|.
    // NOTE: |task| might be silently ignored (i.e. not executed) and
    // destructed (possibly on the calling thread or on the thread of
    // |executionContext|) when |executionContext| is stopped or
    // CrossThreadHolder is destructed.
    void postTask(std::unique_ptr<WTF::Function<void(T*, ExecutionContext*), WTF::CrossThreadAffinity>> task)
    {
        MutexLocker locker(m_mutex->mutex());
        if (!m_bridge) {
            // The bridge has already disappeared.
            return;
        }
        m_bridge->getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&Bridge::runTask, wrapCrossThreadPersistent(m_bridge.get()), passed(std::move(task))));
    }

    ~CrossThreadHolder()
    {
        MutexLocker locker(m_mutex->mutex());
        clearInternal();
    }

private:
    // Object graph:
    //                     +------+                          +-----------------+
    //     T <-unique_ptr- |Bridge| ---------*-------------> |CrossThreadHolder|
    //                     |      | <-CrossThreadPersistent- |                 |
    //                     +------+                          +-----------------+
    //                        |                                    |
    //                        +--RefPtr--> MutexWrapper <--RefPtr--+
    // The CrossThreadPersistent<T>/T* between CrossThreadHolder and Bridge
    // are protected by MutexWrapper
    // and cleared when CrossThreadHolder::clearInternal() is called, i.e.:
    // [1] when |executionContext| is stopped, or
    // [2] when CrossThreadHolder is destructed.
    // Then Bridge is shortly garbage collected and T is destructed.

    class MutexWrapper : public ThreadSafeRefCounted<MutexWrapper> {
    public:
        static PassRefPtr<MutexWrapper> create() { return adoptRef(new MutexWrapper()); }
        Mutex& mutex() { return m_mutex; }
    private:
        MutexWrapper() = default;
        Mutex m_mutex;
    };

    // All methods except for clearInternal()
    // must be called on |executionContext|'s thread.
    class Bridge
        : public GarbageCollectedFinalized<Bridge>
        , public ActiveDOMObject {
        USING_GARBAGE_COLLECTED_MIXIN(Bridge);
    public:
        Bridge(ExecutionContext* executionContext, std::unique_ptr<T> obj, PassRefPtr<MutexWrapper> mutex, CrossThreadHolder* holder)
            : ActiveDOMObject(executionContext)
            , m_obj(std::move(obj))
            , m_mutex(mutex)
            , m_holder(holder)
        {
            suspendIfNeeded();
        }

        DEFINE_INLINE_TRACE()
        {
            ActiveDOMObject::trace(visitor);
        }

        T* getObject() const { return m_obj.get(); }

        // Must be protected by |m_mutex|.
        // Is called from CrossThreadHolder::clearInternal() and
        // can be called on any thread.
        void clearInternal()
        {
            // We don't clear |m_obj| here to destruct |m_obj| on the thread
            // of |executionContext|.
            m_holder = nullptr;
        }

        void runTask(std::unique_ptr<WTF::Function<void(T*, ExecutionContext*), WTF::CrossThreadAffinity>> task)
        {
            ASSERT(getExecutionContext()->isContextThread());
            if (m_obj)
                (*task)(m_obj.get(), getExecutionContext());
        }

    private:
        // ActiveDOMObject
        void stop() override
        {
            ASSERT(getExecutionContext()->isContextThread());
            {
                MutexLocker locker(m_mutex->mutex());
                if (m_holder)
                    m_holder->clearInternal();
                ASSERT(!m_holder);
            }

            // We have to destruct |*m_obj| here because destructing |*m_obj|
            // in ~Bridge() might be too late when |executionContext| is
            // stopped.
            m_obj.reset();
        }


        std::unique_ptr<T> m_obj;
        // All accesses to |m_holder| must be protected by |m_mutex|.
        RefPtr<MutexWrapper> m_mutex;
        CrossThreadHolder* m_holder;
    };

    // Must be protected by |m_mutex|.
    void clearInternal()
    {
        if (m_bridge)
            m_bridge->clearInternal();
        m_bridge.clear();
    }

    CrossThreadHolder(ExecutionContext* executionContext, std::unique_ptr<T> obj)
        : m_mutex(MutexWrapper::create())
        , m_bridge(new Bridge(executionContext, std::move(obj), m_mutex, this))
    {
    }

    RefPtr<MutexWrapper> m_mutex;
    // |m_bridge| is protected by |m_mutex|.
    // |m_bridge| is cleared before the thread that allocated |*m_bridge|
    // is stopped.
    CrossThreadPersistent<Bridge> m_bridge;
};

} // namespace blink

#endif // CrossThreadHolder_h
