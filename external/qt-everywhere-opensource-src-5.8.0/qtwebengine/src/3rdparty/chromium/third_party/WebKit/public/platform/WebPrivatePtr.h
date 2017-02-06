/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPrivatePtr_h
#define WebPrivatePtr_h

#include "WebCommon.h"
#include "base/logging.h"

#if INSIDE_BLINK
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/TypeTraits.h"
#endif

namespace WTF {
template<class T> class ThreadSafeRefCounted;
}

namespace blink {

// By default, the destruction of a WebPrivatePtr<> must happen on the same
// thread that created it, but can optionally be allowed to happen on
// another thread.
enum WebPrivatePtrDestruction {
    WebPrivatePtrDestructionSameThread,
    WebPrivatePtrDestructionCrossThread,
};

// The WebPrivatePtr<> holds by default a strong reference to its Blink object,
// but Blink GC managed objects also support keeping a weak reference by
// way of WebPrivatePtr<>.
enum class WebPrivatePtrStrength {
    Normal,
    Weak,
};

#if INSIDE_BLINK
enum LifetimeManagementType {
    RefCountedLifetime,
    GarbageCollectedLifetime,
};

template<typename T>
struct LifetimeOf {
private:
    static const bool isGarbageCollected = WTF::IsSubclassOfTemplate<T, GarbageCollected>::value || IsGarbageCollectedMixin<T>::value;
public:
    static const LifetimeManagementType value = !isGarbageCollected ? RefCountedLifetime : GarbageCollectedLifetime;
};

template<typename T, WebPrivatePtrDestruction crossThreadDestruction, WebPrivatePtrStrength strongOrWeak, LifetimeManagementType lifetime>
class PtrStorageImpl;

template<typename T, WebPrivatePtrDestruction crossThreadDestruction, WebPrivatePtrStrength strongOrWeak>
class PtrStorageImpl<T, crossThreadDestruction, strongOrWeak, RefCountedLifetime> {
public:
    typedef PassRefPtr<T> BlinkPtrType;

    void assign(const BlinkPtrType& val)
    {
        static_assert(crossThreadDestruction == WebPrivatePtrDestructionSameThread || WTF::IsSubclassOfTemplate<T, WTF::ThreadSafeRefCounted>::value, "Cross thread destructible class must derive from ThreadSafeRefCounted<>");
        static_assert(strongOrWeak == WebPrivatePtrStrength::Normal, "Ref-counted classes do not support weak WebPrivatePtr<> references");
        release();
        m_ptr = val.leakRef();
    }

    void assign(const PtrStorageImpl& other)
    {
        T* val = other.get();
        if (m_ptr == val)
            return;
        release();
        WTF::refIfNotNull(val);
        m_ptr = val;
    }

    T* get() const { return m_ptr; }

    void release()
    {
        WTF::derefIfNotNull(m_ptr);
        m_ptr = 0;
    }

private:
    T* m_ptr;
};

template <typename T, WebPrivatePtrDestruction, WebPrivatePtrStrength>
struct WebPrivatePtrPersistentStorageType {
public:
    using Type = Persistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T, WebPrivatePtrDestructionSameThread, WebPrivatePtrStrength::Weak>  {
public:
    using Type = WeakPersistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T, WebPrivatePtrDestructionCrossThread, WebPrivatePtrStrength::Normal> {
public:
    using Type = CrossThreadPersistent<T>;
};

template <typename T>
struct WebPrivatePtrPersistentStorageType<T, WebPrivatePtrDestructionCrossThread, WebPrivatePtrStrength::Weak> {
public:
    using Type = CrossThreadWeakPersistent<T>;
};

template<typename T, WebPrivatePtrDestruction crossThreadDestruction, WebPrivatePtrStrength strongOrWeak>
class PtrStorageImpl<T, crossThreadDestruction, strongOrWeak, GarbageCollectedLifetime> {
public:
    void assign(T* val)
    {
        if (!val) {
            release();
            return;
        }

        if (!m_handle)
            m_handle = new (typename WebPrivatePtrPersistentStorageType<T, crossThreadDestruction, strongOrWeak>::Type)();

        (*m_handle) = val;
    }

    template<typename U> void assign(U* val) { assign(static_cast<T*>(val)); }

    void assign(const PtrStorageImpl& other) { assign(other.get()); }

    T* get() const { return m_handle ? m_handle->get() : 0; }

    void release()
    {
        delete m_handle;
        m_handle = 0;
    }

private:
    typename WebPrivatePtrPersistentStorageType<T, crossThreadDestruction, strongOrWeak>::Type* m_handle;
};

template<typename T, WebPrivatePtrDestruction crossThreadDestruction, WebPrivatePtrStrength strongOrWeak>
class PtrStorage : public PtrStorageImpl<T, crossThreadDestruction, strongOrWeak, LifetimeOf<T>::value> {
public:
    static PtrStorage& fromSlot(void** slot)
    {
        static_assert(sizeof(PtrStorage) == sizeof(void*), "PtrStorage must be the size of a pointer");
        return *reinterpret_cast<PtrStorage*>(slot);
    }

    static const PtrStorage& fromSlot(void* const* slot)
    {
        static_assert(sizeof(PtrStorage) == sizeof(void*), "PtrStorage must be the size of a pointer");
        return *reinterpret_cast<const PtrStorage*>(slot);
    }

private:
    // Prevent construction via normal means.
    PtrStorage();
    PtrStorage(const PtrStorage&);
};
#endif

// This class is an implementation detail of the Blink API. It exists to help
// simplify the implementation of Blink interfaces that merely wrap a reference
// counted WebCore class.
//
// A typical implementation of a class which uses WebPrivatePtr might look like
// this:
//    class WebFoo {
//    public:
//        BLINK_EXPORT ~WebFoo();
//        WebFoo() { }
//        WebFoo(const WebFoo& other) { assign(other); }
//        WebFoo& operator=(const WebFoo& other)
//        {
//            assign(other);
//            return *this;
//        }
//        BLINK_EXPORT void assign(const WebFoo&);  // Implemented in the body.
//
//        // Methods that are exposed to Chromium and which are specific to
//        // WebFoo go here.
//        BLINK_EXPORT doWebFooThing();
//
//        // Methods that are used only by other Blink classes should only be
//        // declared when INSIDE_BLINK is set.
//    #if INSIDE_BLINK
//        WebFoo(const WTF::PassRefPtr<Foo>&);
//    #endif
//
//    private:
//        WebPrivatePtr<Foo> m_private;
//    };
//
//    // WebFoo.cpp
//    WebFoo::~WebFoo() { m_private.reset(); }
//    void WebFoo::assign(const WebFoo& other) { ... }
//
template <typename T, WebPrivatePtrDestruction crossThreadDestruction = WebPrivatePtrDestructionSameThread, WebPrivatePtrStrength strongOrWeak = WebPrivatePtrStrength::Normal>
class WebPrivatePtr {
public:
    WebPrivatePtr() : m_storage(0) { }
    ~WebPrivatePtr()
    {
        // We don't destruct the object pointed by m_ptr here because we don't
        // want to expose destructors of core classes to embedders. We should
        // call reset() manually in destructors of classes with WebPrivatePtr
        // members.
        DCHECK(!m_storage);
    }

    bool isNull() const { return !m_storage; }

#if INSIDE_BLINK
    template<typename U>
    WebPrivatePtr(const U& ptr)
        : m_storage(0)
    {
        storage().assign(ptr);
    }

    void reset() { storage().release(); }

    WebPrivatePtr& operator=(const WebPrivatePtr& other)
    {
        storage().assign(other.storage());
        return *this;
    }

    template<typename U>
    WebPrivatePtr& operator=(const U& ptr)
    {
        storage().assign(ptr);
        return *this;
    }

    T* get() const { return storage().get(); }

    T& operator*() const
    {
        ASSERT(m_storage);
        return *get();
    }

    T* operator->() const
    {
        ASSERT(m_storage);
        return get();
    }
#endif

private:
#if INSIDE_BLINK
    PtrStorage<T, crossThreadDestruction, strongOrWeak>& storage() { return PtrStorage<T, crossThreadDestruction, strongOrWeak>::fromSlot(&m_storage); }
    const PtrStorage<T, crossThreadDestruction, strongOrWeak>& storage() const { return PtrStorage<T, crossThreadDestruction, strongOrWeak>::fromSlot(&m_storage); }
#endif

#if !INSIDE_BLINK
    // Disable the assignment operator; we define it above for when
    // INSIDE_BLINK is set, but we need to make sure that it is not
    // used outside there; the compiler-provided version won't handle reference
    // counting properly.
    WebPrivatePtr& operator=(const WebPrivatePtr& other);
#endif
    // Disable the copy constructor; classes that contain a WebPrivatePtr
    // should implement their copy constructor using assign().
    WebPrivatePtr(const WebPrivatePtr&);

    void* m_storage;
};

} // namespace blink

#endif // WebPrivatePtr_h
