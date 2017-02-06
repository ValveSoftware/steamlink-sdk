// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Persistent_h
#define Persistent_h

#include "platform/heap/Heap.h"
#include "platform/heap/Member.h"
#include "platform/heap/PersistentNode.h"
#include "platform/heap/Visitor.h"
#include "wtf/Allocator.h"
#include "wtf/Atomics.h"

namespace blink {

// Marker used to annotate persistent objects and collections with,
// so as to enable reliable testing for persistent references via
// a type trait (see TypeTraits.h's IsPersistentReferenceType<>.)
#define IS_PERSISTENT_REFERENCE_TYPE()               \
    public:                                          \
        using IsPersistentReferenceTypeMarker = int; \
    private:

enum WeaknessPersistentConfiguration {
    NonWeakPersistentConfiguration,
    WeakPersistentConfiguration
};

enum CrossThreadnessPersistentConfiguration {
    SingleThreadPersistentConfiguration,
    CrossThreadPersistentConfiguration
};

template<typename T, WeaknessPersistentConfiguration weaknessConfiguration, CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
class PersistentBase {
    USING_FAST_MALLOC(PersistentBase);
    IS_PERSISTENT_REFERENCE_TYPE();
public:
    PersistentBase() : m_raw(nullptr)
    {
        initialize();
    }

    PersistentBase(std::nullptr_t) : m_raw(nullptr)
    {
        initialize();
    }

    PersistentBase(T* raw) : m_raw(raw)
    {
        initialize();
        checkPointer();
    }

    PersistentBase(T& raw) : m_raw(&raw)
    {
        initialize();
        checkPointer();
    }

    PersistentBase(const PersistentBase& other) : m_raw(other)
    {
        initialize();
        checkPointer();
    }

    template<typename U>
    PersistentBase(const PersistentBase<U, weaknessConfiguration, crossThreadnessConfiguration>& other) : m_raw(other)
    {
        initialize();
        checkPointer();
    }

    template<typename U>
    PersistentBase(const Member<U>& other) : m_raw(other)
    {
        initialize();
        checkPointer();
    }

    PersistentBase(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1))
    {
        initialize();
        checkPointer();
    }

    ~PersistentBase()
    {
        uninitialize();
        m_raw = nullptr;
    }

    bool isHashTableDeletedValue() const { return m_raw == reinterpret_cast<T*>(-1); }

    T* release()
    {
        T* result = m_raw;
        assign(nullptr);
        return result;
    }

    void clear() { assign(nullptr); }
    T& operator*() const { return *m_raw; }
    explicit operator bool() const { return m_raw; }
    operator T*() const { return m_raw; }
    T* operator->() const { return *this; }
    T* get() const { return m_raw; }

    template<typename U>
    PersistentBase& operator=(U* other)
    {
        assign(other);
        return *this;
    }

    PersistentBase& operator=(std::nullptr_t)
    {
        assign(nullptr);
        return *this;
    }

    PersistentBase& operator=(const PersistentBase& other)
    {
        assign(other);
        return *this;
    }

    template<typename U>
    PersistentBase& operator=(const PersistentBase<U, weaknessConfiguration, crossThreadnessConfiguration>& other)
    {
        assign(other);
        return *this;
    }

    template<typename U>
    PersistentBase& operator=(const Member<U>& other)
    {
        assign(other);
        return *this;
    }

    // Register the persistent node as a 'static reference',
    // belonging to the current thread and a persistent that must
    // be cleared when the ThreadState itself is cleared out and
    // destructed.
    //
    // Static singletons arrange for this to happen, either to ensure
    // clean LSan leak reports or to register a thread-local persistent
    // needing to be cleared out before the thread is terminated.
    PersistentBase* registerAsStaticReference()
    {
        if (m_persistentNode) {
            ASSERT(ThreadState::current());
            ThreadState::current()->registerStaticPersistentNode(m_persistentNode, nullptr);
            LEAK_SANITIZER_IGNORE_OBJECT(this);
        }
        return this;
    }

protected:
    NO_SANITIZE_ADDRESS
    T* atomicGet() { return reinterpret_cast<T*>(acquireLoad(reinterpret_cast<void* volatile*>(&m_raw))); }

private:
    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    void assign(T* ptr)
    {
        if (crossThreadnessConfiguration == CrossThreadPersistentConfiguration)
            releaseStore(reinterpret_cast<void* volatile*>(&m_raw), ptr);
        else
            m_raw = ptr;
        checkPointer();
        if (m_raw) {
            if (!m_persistentNode)
                initialize();
            return;
        }
        uninitialize();
    }

    template<typename VisitorDispatcher>
    void tracePersistent(VisitorDispatcher visitor)
    {
        static_assert(sizeof(T), "T must be fully defined");
        static_assert(IsGarbageCollectedType<T>::value, "T needs to be a garbage collected object");
        if (weaknessConfiguration == WeakPersistentConfiguration) {
            if (crossThreadnessConfiguration == CrossThreadPersistentConfiguration)
                visitor->registerWeakCellWithCallback(reinterpret_cast<void**>(this), handleWeakPersistent);
            else
                visitor->registerWeakMembers(this, m_raw, handleWeakPersistent);
        } else {
            visitor->mark(m_raw);
        }
    }

    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    void initialize()
    {
        ASSERT(!m_persistentNode);
        if (!m_raw || isHashTableDeletedValue())
            return;

        TraceCallback traceCallback = TraceMethodDelegate<PersistentBase, &PersistentBase::tracePersistent>::trampoline;
        if (crossThreadnessConfiguration == CrossThreadPersistentConfiguration) {
            ProcessHeap::crossThreadPersistentRegion().allocatePersistentNode(m_persistentNode, this, traceCallback);
            return;
        }
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->checkThread());
        m_persistentNode = state->getPersistentRegion()->allocatePersistentNode(this, traceCallback);
#if ENABLE(ASSERT)
        m_state = state;
#endif
    }

    void uninitialize()
    {
        if (crossThreadnessConfiguration == CrossThreadPersistentConfiguration) {
            if (acquireLoad(reinterpret_cast<void* volatile*>(&m_persistentNode)))
                ProcessHeap::crossThreadPersistentRegion().freePersistentNode(m_persistentNode);
            return;
        }

        if (!m_persistentNode)
            return;
        ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
        ASSERT(state->checkThread());
        // Persistent handle must be created and destructed in the same thread.
        ASSERT(m_state == state);
        state->freePersistentNode(m_persistentNode);
        m_persistentNode = nullptr;
    }

    void checkPointer()
    {
#if ENABLE(ASSERT) && defined(ADDRESS_SANITIZER)
        if (!m_raw || isHashTableDeletedValue())
            return;

        // ThreadHeap::isHeapObjectAlive(m_raw) checks that m_raw is a traceable
        // object. In other words, it checks that the pointer is either of:
        //
        //   (a) a pointer to the head of an on-heap object.
        //   (b) a pointer to the head of an on-heap mixin object.
        //
        // Otherwise, ThreadHeap::isHeapObjectAlive will crash when it calls
        // header->checkHeader().
        ThreadHeap::isHeapObjectAlive(m_raw);
#endif
    }

    static void handleWeakPersistent(Visitor* self, void* persistentPointer)
    {
        using Base = PersistentBase<typename std::remove_const<T>::type, weaknessConfiguration, crossThreadnessConfiguration>;
        Base* persistent = reinterpret_cast<Base*>(persistentPointer);
        T* object = persistent->get();
        if (object && !ObjectAliveTrait<T>::isHeapObjectAlive(object))
            persistent->clear();
    }

    // m_raw is accessed most, so put it at the first field.
    T* m_raw;
    PersistentNode* m_persistentNode = nullptr;
#if ENABLE(ASSERT)
    ThreadState* m_state = nullptr;
#endif
};

// Persistent is a way to create a strong pointer from an off-heap object
// to another on-heap object. As long as the Persistent handle is alive
// the GC will keep the object pointed to alive. The Persistent handle is
// always a GC root from the point of view of the GC.
//
// We have to construct and destruct Persistent in the same thread.
template<typename T>
class Persistent : public PersistentBase<T, NonWeakPersistentConfiguration, SingleThreadPersistentConfiguration> {
    typedef PersistentBase<T, NonWeakPersistentConfiguration, SingleThreadPersistentConfiguration> Parent;
public:
    Persistent() : Parent() { }
    Persistent(std::nullptr_t) : Parent(nullptr) { }
    Persistent(T* raw) : Parent(raw) { }
    Persistent(T& raw) : Parent(raw) { }
    Persistent(const Persistent& other) : Parent(other) { }
    template<typename U>
    Persistent(const Persistent<U>& other) : Parent(other) { }
    template<typename U>
    Persistent(const Member<U>& other) : Parent(other) { }
    Persistent(WTF::HashTableDeletedValueType x) : Parent(x) { }

    template<typename U>
    Persistent& operator=(U* other)
    {
        Parent::operator=(other);
        return *this;
    }

    Persistent& operator=(std::nullptr_t)
    {
        Parent::operator=(nullptr);
        return *this;
    }

    Persistent& operator=(const Persistent& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Persistent<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Member<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }
};

// WeakPersistent is a way to create a weak pointer from an off-heap object
// to an on-heap object. The m_raw is automatically cleared when the pointee
// gets collected.
//
// We have to construct and destruct WeakPersistent in the same thread.
//
// Note that collections of WeakPersistents are not supported. Use a persistent
// collection of WeakMembers instead.
//
//   HashSet<WeakPersistent<T>> m_set; // wrong
//   PersistentHeapHashSet<WeakMember<T>> m_set; // correct
template<typename T>
class WeakPersistent : public PersistentBase<T, WeakPersistentConfiguration, SingleThreadPersistentConfiguration> {
    typedef PersistentBase<T, WeakPersistentConfiguration, SingleThreadPersistentConfiguration> Parent;
public:
    WeakPersistent() : Parent() { }
    WeakPersistent(std::nullptr_t) : Parent(nullptr) { }
    WeakPersistent(T* raw) : Parent(raw) { }
    WeakPersistent(T& raw) : Parent(raw) { }
    WeakPersistent(const WeakPersistent& other) : Parent(other) { }
    template<typename U>
    WeakPersistent(const WeakPersistent<U>& other) : Parent(other) { }
    template<typename U>
    WeakPersistent(const Member<U>& other) : Parent(other) { }

    template<typename U>
    WeakPersistent& operator=(U* other)
    {
        Parent::operator=(other);
        return *this;
    }

    WeakPersistent& operator=(std::nullptr_t)
    {
        Parent::operator=(nullptr);
        return *this;
    }

    WeakPersistent& operator=(const WeakPersistent& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    WeakPersistent& operator=(const WeakPersistent<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    WeakPersistent& operator=(const Member<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }
};

// Unlike Persistent, we can destruct a CrossThreadPersistent in a thread
// different from the construction thread.
template<typename T>
class CrossThreadPersistent : public PersistentBase<T, NonWeakPersistentConfiguration, CrossThreadPersistentConfiguration> {
    typedef PersistentBase<T, NonWeakPersistentConfiguration, CrossThreadPersistentConfiguration> Parent;
public:
    CrossThreadPersistent() : Parent() { }
    CrossThreadPersistent(std::nullptr_t) : Parent(nullptr) { }
    CrossThreadPersistent(T* raw) : Parent(raw) { }
    CrossThreadPersistent(T& raw) : Parent(raw) { }
    CrossThreadPersistent(const CrossThreadPersistent& other) : Parent(other) { }
    template<typename U>
    CrossThreadPersistent(const CrossThreadPersistent<U>& other) : Parent(other) { }
    template<typename U>
    CrossThreadPersistent(const Member<U>& other) : Parent(other) { }
    CrossThreadPersistent(WTF::HashTableDeletedValueType x) : Parent(x) { }

    T* atomicGet() { return Parent::atomicGet(); }

    template<typename U>
    CrossThreadPersistent& operator=(U* other)
    {
        Parent::operator=(other);
        return *this;
    }

    CrossThreadPersistent& operator=(std::nullptr_t)
    {
        Parent::operator=(nullptr);
        return *this;
    }

    CrossThreadPersistent& operator=(const CrossThreadPersistent& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    CrossThreadPersistent& operator=(const CrossThreadPersistent<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    CrossThreadPersistent& operator=(const Member<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }
};

// Combines the behavior of CrossThreadPersistent and WeakPersistent.
template<typename T>
class CrossThreadWeakPersistent : public PersistentBase<T, WeakPersistentConfiguration, CrossThreadPersistentConfiguration> {
    typedef PersistentBase<T, WeakPersistentConfiguration, CrossThreadPersistentConfiguration> Parent;
public:
    CrossThreadWeakPersistent() : Parent() { }
    CrossThreadWeakPersistent(std::nullptr_t) : Parent(nullptr) { }
    CrossThreadWeakPersistent(T* raw) : Parent(raw) { }
    CrossThreadWeakPersistent(T& raw) : Parent(raw) { }
    CrossThreadWeakPersistent(const CrossThreadWeakPersistent& other) : Parent(other) { }
    template<typename U>
    CrossThreadWeakPersistent(const CrossThreadWeakPersistent<U>& other) : Parent(other) { }
    template<typename U>
    CrossThreadWeakPersistent(const Member<U>& other) : Parent(other) { }

    template<typename U>
    CrossThreadWeakPersistent& operator=(U* other)
    {
        Parent::operator=(other);
        return *this;
    }

    CrossThreadWeakPersistent& operator=(std::nullptr_t)
    {
        Parent::operator=(nullptr);
        return *this;
    }

    CrossThreadWeakPersistent& operator=(const CrossThreadWeakPersistent& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    CrossThreadWeakPersistent& operator=(const CrossThreadWeakPersistent<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }

    template<typename U>
    CrossThreadWeakPersistent& operator=(const Member<U>& other)
    {
        Parent::operator=(other);
        return *this;
    }
};

template<typename Collection>
class PersistentHeapCollectionBase : public Collection {
    // We overload the various new and delete operators with using the WTF PartitionAllocator to ensure persistent
    // heap collections are always allocated off-heap. This allows persistent collections to be used in
    // DEFINE_STATIC_LOCAL et. al.
    WTF_USE_ALLOCATOR(PersistentHeapCollectionBase, WTF::PartitionAllocator);
    IS_PERSISTENT_REFERENCE_TYPE();
public:
    PersistentHeapCollectionBase()
    {
        initialize();
    }

    PersistentHeapCollectionBase(const PersistentHeapCollectionBase& other) : Collection(other)
    {
        initialize();
    }

    template<typename OtherCollection>
    PersistentHeapCollectionBase(const OtherCollection& other) : Collection(other)
    {
        initialize();
    }

    ~PersistentHeapCollectionBase()
    {
        uninitialize();
    }

    // See PersistentBase::registerAsStaticReference() comment.
    PersistentHeapCollectionBase* registerAsStaticReference()
    {
        if (m_persistentNode) {
            ASSERT(ThreadState::current());
            ThreadState::current()->registerStaticPersistentNode(m_persistentNode, &PersistentHeapCollectionBase<Collection>::clearPersistentNode);
            LEAK_SANITIZER_IGNORE_OBJECT(this);
        }
        return this;
    }

private:

    template<typename VisitorDispatcher>
    void tracePersistent(VisitorDispatcher visitor)
    {
        static_assert(sizeof(Collection), "Collection must be fully defined");
        visitor->trace(*static_cast<Collection*>(this));
    }

    // Used when the registered PersistentNode of this object is
    // released during ThreadState shutdown, clearing the association.
    static void clearPersistentNode(void *self)
    {
        PersistentHeapCollectionBase<Collection>* collection = (reinterpret_cast<PersistentHeapCollectionBase<Collection>*>(self));
        collection->uninitialize();
        collection->clear();
    }

    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    void initialize()
    {
        // FIXME: Derive affinity based on the collection.
        ThreadState* state = ThreadState::current();
        ASSERT(state->checkThread());
        m_persistentNode = state->getPersistentRegion()->allocatePersistentNode(this, TraceMethodDelegate<PersistentHeapCollectionBase<Collection>, &PersistentHeapCollectionBase<Collection>::tracePersistent>::trampoline);
#if ENABLE(ASSERT)
        m_state = state;
#endif
    }

    void uninitialize()
    {
        if (!m_persistentNode)
            return;
        ThreadState* state = ThreadState::current();
        ASSERT(state->checkThread());
        // Persistent handle must be created and destructed in the same thread.
        ASSERT(m_state == state);
        state->freePersistentNode(m_persistentNode);
        m_persistentNode = nullptr;
    }

    PersistentNode* m_persistentNode;
#if ENABLE(ASSERT)
    ThreadState* m_state;
#endif
};

template<
    typename KeyArg,
    typename MappedArg,
    typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>,
    typename MappedTraitsArg = HashTraits<MappedArg>>
class PersistentHeapHashMap : public PersistentHeapCollectionBase<HeapHashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>> { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class PersistentHeapHashSet : public PersistentHeapCollectionBase<HeapHashSet<ValueArg, HashArg, TraitsArg>> { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg>>
class PersistentHeapLinkedHashSet : public PersistentHeapCollectionBase<HeapLinkedHashSet<ValueArg, HashArg, TraitsArg>> { };

template<
    typename ValueArg,
    size_t inlineCapacity = 0,
    typename HashArg = typename DefaultHash<ValueArg>::Hash>
class PersistentHeapListHashSet : public PersistentHeapCollectionBase<HeapListHashSet<ValueArg, inlineCapacity, HashArg>> { };

template<
    typename ValueArg,
    typename HashFunctions = typename DefaultHash<ValueArg>::Hash,
    typename Traits = HashTraits<ValueArg>>
class PersistentHeapHashCountedSet : public PersistentHeapCollectionBase<HeapHashCountedSet<ValueArg, HashFunctions, Traits>> { };

template<typename T, size_t inlineCapacity = 0>
class PersistentHeapVector : public PersistentHeapCollectionBase<HeapVector<T, inlineCapacity>> {
public:
    PersistentHeapVector()
    {
        initializeUnusedSlots();
    }

    explicit PersistentHeapVector(size_t size)
        : PersistentHeapCollectionBase<HeapVector<T, inlineCapacity>>(size)
    {
        initializeUnusedSlots();
    }

    PersistentHeapVector(const PersistentHeapVector& other)
        : PersistentHeapCollectionBase<HeapVector<T, inlineCapacity>>(other)
    {
        initializeUnusedSlots();
    }

    template<size_t otherCapacity>
    PersistentHeapVector(const HeapVector<T, otherCapacity>& other)
        : PersistentHeapCollectionBase<HeapVector<T, inlineCapacity>>(other)
    {
        initializeUnusedSlots();
    }

private:
    void initializeUnusedSlots()
    {
        // The PersistentHeapVector is allocated off heap along with its
        // inline buffer (if any.) Maintain the invariant that unused
        // slots are cleared for the off-heap inline buffer also.
        size_t unusedSlots = this->capacity() - this->size();
        if (unusedSlots)
            this->clearUnusedSlots(this->end(), this->end() + unusedSlots);
    }
};

template<typename T, size_t inlineCapacity = 0>
class PersistentHeapDeque : public PersistentHeapCollectionBase<HeapDeque<T, inlineCapacity>> {
public:
    PersistentHeapDeque() { }

    template<size_t otherCapacity>
    PersistentHeapDeque(const HeapDeque<T, otherCapacity>& other)
        : PersistentHeapCollectionBase<HeapDeque<T, inlineCapacity>>(other)
    {
    }
};

template <typename T>
Persistent<T> wrapPersistent(T* value)
{
    return Persistent<T>(value);
}

template <typename T>
WeakPersistent<T> wrapWeakPersistent(T* value)
{
    return WeakPersistent<T>(value);
}

template <typename T>
CrossThreadPersistent<T> wrapCrossThreadPersistent(T* value)
{
    return CrossThreadPersistent<T>(value);
}

template <typename T>
CrossThreadWeakPersistent<T> wrapCrossThreadWeakPersistent(T* value)
{
    return CrossThreadWeakPersistent<T>(value);
}

// Comparison operators between (Weak)Members, Persistents, and UntracedMembers.
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }

template<typename T, typename U> inline bool operator==(const Member<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Member<U>& b) { return a.get() != b.get(); }

} // namespace blink

namespace WTF {

template <typename T>
struct DefaultHash<blink::Persistent<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakPersistent<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadPersistent<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadWeakPersistent<T>> {
    STATIC_ONLY(DefaultHash);
    using Hash = MemberHash<T>;
};

} // namespace WTF

namespace base {

template <typename T>
struct IsWeakReceiver<blink::WeakPersistent<T>> : std::true_type {};

template <typename T>
struct IsWeakReceiver<blink::CrossThreadWeakPersistent<T>> : std::true_type {};

}

#endif // Persistent_h
