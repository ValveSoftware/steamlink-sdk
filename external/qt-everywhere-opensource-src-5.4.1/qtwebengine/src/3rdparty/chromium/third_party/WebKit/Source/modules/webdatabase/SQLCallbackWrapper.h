/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SQLCallbackWrapper_h
#define SQLCallbackWrapper_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {

// A helper class to safely dereference the callback objects held by
// SQLStatement and SQLTransaction on the proper thread. The 'wrapped'
// callback is dereferenced:
// - by destructing the enclosing wrapper - on any thread
// - by calling clear() - on any thread
// - by unwrapping and then dereferencing normally - on context thread only
// Oilpan: ~T must be thread-independent.
template<typename T> class SQLCallbackWrapper {
    DISALLOW_ALLOCATION();
public:
    SQLCallbackWrapper(PassOwnPtr<T> callback, ExecutionContext* executionContext)
        : m_callback(callback)
        , m_executionContext(m_callback ? executionContext : 0)
    {
        ASSERT(!m_callback || (m_executionContext.get() && m_executionContext->isContextThread()));
    }

    ~SQLCallbackWrapper()
    {
        clear();
    }

    void trace(Visitor* visitor) { visitor->trace(m_executionContext); }

    void clear()
    {
#if ENABLE(OILPAN)
        // It's safe to call ~T in non-context-thread.
        // Implementation classes of ExecutionContext are on-heap. Their
        // destructors are called in their owner threads.
        MutexLocker locker(m_mutex);
        m_callback.clear();
        m_executionContext.clear();
#else
        ExecutionContext* context;
        OwnPtr<T> callback;
        {
            MutexLocker locker(m_mutex);
            if (!m_callback) {
                ASSERT(!m_executionContext);
                return;
            }
            if (m_executionContext->isContextThread()) {
                m_callback.clear();
                m_executionContext.clear();
                return;
            }
            context = m_executionContext.release().leakRef();
            callback = m_callback.release();
        }
        context->postTask(SafeReleaseTask::create(callback.release()));
#endif
    }

    PassOwnPtr<T> unwrap()
    {
        MutexLocker locker(m_mutex);
        ASSERT(!m_callback || m_executionContext->isContextThread());
        m_executionContext.clear();
        return m_callback.release();
    }

    // Useful for optimizations only, please test the return value of unwrap to be sure.
    bool hasCallback() const { return m_callback; }

private:
#if !ENABLE(OILPAN)
    class SafeReleaseTask : public ExecutionContextTask {
    public:
        static PassOwnPtr<SafeReleaseTask> create(PassOwnPtr<T> callbackToRelease)
        {
            return adoptPtr(new SafeReleaseTask(callbackToRelease));
        }

        virtual void performTask(ExecutionContext* context)
        {
            ASSERT(m_callbackToRelease && context && context->isContextThread());
            m_callbackToRelease.clear();
            context->deref();
        }

        virtual bool isCleanupTask() const { return true; }

    private:
        explicit SafeReleaseTask(PassOwnPtr<T> callbackToRelease)
            : m_callbackToRelease(callbackToRelease)
        {
        }

        OwnPtr<T> m_callbackToRelease;
    };
#endif

    Mutex m_mutex;
    OwnPtr<T> m_callback;
    RefPtrWillBeMember<ExecutionContext> m_executionContext;
};

} // namespace WebCore

#endif // SQLCallbackWrapper_h
