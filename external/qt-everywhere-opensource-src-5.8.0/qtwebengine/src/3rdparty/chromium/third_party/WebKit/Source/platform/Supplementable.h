/*
 * Copyright (C) 2012 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Supplementable_h
#define Supplementable_h

#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

#if ENABLE(ASSERT)
#include "wtf/Threading.h"
#endif

namespace blink {

// What you should know about Supplementable and Supplement
// ========================================================
// Supplementable and Supplement instances are meant to be thread local. They
// should only be accessed from within the thread that created them. The
// 2 classes are not designed for safe access from another thread. Violating
// this design assumption can result in memory corruption and unpredictable
// behavior.
//
// What you should know about the Supplement keys
// ==============================================
// The Supplement is expected to use the same const char* string instance
// as its key. The Supplementable's SupplementMap will use the address of the
// string as the key and not the characters themselves. Hence, 2 strings with
// the same characters will be treated as 2 different keys.
//
// In practice, it is recommended that Supplements implements a static method
// for returning its key to use. For example:
//
//     class MyClass : public Supplement<MySupplementable> {
//         ...
//         static const char* supplementName();
//     }
//
//     const char* MyClass::supplementName()
//     {
//         return "MyClass";
//     }
//
// An example of the using the key:
//
//     MyClass* MyClass::from(MySupplementable* host)
//     {
//         return static_cast<MyClass*>(Supplement<MySupplementable>::from(host, supplementName()));
//     }
//
// What you should know about thread checks
// ========================================
// When assertion is enabled this class performs thread-safety check so that
// provideTo and from happen on the same thread. If you want to provide
// some value for Workers this thread check may not work very well though,
// since in most case you'd provide the value while worker preparation is
// being done on the main thread, even before the worker thread is started.
// If that's the case you can explicitly call reattachThread() when the
// Supplementable object is passed to the final destination thread (i.e.
// worker thread). Please be extremely careful to use the method though,
// as randomly calling the method could easily cause racy condition.
//
// Note that reattachThread() does nothing if assertion is not enabled.
//

template<typename T>
class Supplementable;

template<typename T>
class Supplement : public GarbageCollectedMixin {
public:
    static void provideTo(Supplementable<T>& host, const char* key, Supplement<T>* supplement)
    {
        host.provideSupplement(key, supplement);
    }

    static Supplement<T>* from(Supplementable<T>& host, const char* key)
    {
        return host.requireSupplement(key);
    }

    static Supplement<T>* from(Supplementable<T>* host, const char* key)
    {
        return host ? host->requireSupplement(key) : 0;
    }
};

// Supplementable<T> inherits from GarbageCollectedMixin virtually
// to allow ExecutionContext to derive from two GC mixin classes.
template<typename T>
class Supplementable : public virtual GarbageCollectedMixin {
    WTF_MAKE_NONCOPYABLE(Supplementable);
public:
    void provideSupplement(const char* key, Supplement<T>* supplement)
    {
        ASSERT(m_threadId == currentThread());
        ASSERT(!this->m_supplements.get(key));
        this->m_supplements.set(key, supplement);
    }

    void removeSupplement(const char* key)
    {
        ASSERT(m_threadId == currentThread());
        this->m_supplements.remove(key);
    }

    Supplement<T>* requireSupplement(const char* key)
    {
        ASSERT(m_threadId == currentThread());
        return this->m_supplements.get(key);
    }

    void reattachThread()
    {
#if ENABLE(ASSERT)
        m_threadId = currentThread();
#endif
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_supplements);
    }

protected:
    using SupplementMap = HeapHashMap<const char*, Member<Supplement<T>>, PtrHash<const char>>;
    SupplementMap m_supplements;

    Supplementable()
#if ENABLE(ASSERT)
        : m_threadId(currentThread())
#endif
    {
    }

#if ENABLE(ASSERT)
private:
    ThreadIdentifier m_threadId;
#endif
};

template<typename T>
struct ThreadingTrait<Supplement<T>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

template<typename T>
struct ThreadingTrait<Supplementable<T>> {
    static const ThreadAffinity Affinity = ThreadingTrait<T>::Affinity;
};

} // namespace blink

#endif // Supplementable_h
