/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef Handle_h
#define Handle_h

#include "platform/heap/Heap.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/Functional.h"
#include "wtf/HashFunctions.h"
#include "wtf/Locker.h"
#include "wtf/RawPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/TypeTraits.h"

namespace WebCore {

template<typename T> class HeapTerminatedArray;

// Template to determine if a class is a GarbageCollectedMixin by checking if it
// has adjustAndMark and isAlive. We can't check directly if the class is a
// GarbageCollectedMixin because casting to it is potentially ambiguous.
template<typename T>
struct IsGarbageCollectedMixin {
    typedef char TrueType;
    struct FalseType {
        char dummy[2];
    };

#if COMPILER(MSVC)
    template<typename U> static TrueType hasAdjustAndMark(char[&U::adjustAndMark != 0]);
    template<typename U> static TrueType hasIsAlive(char[&U::isAlive != 0]);
#else
    template<size_t> struct F;
    template<typename U> static TrueType hasAdjustAndMark(F<sizeof(&U::adjustAndMark)>*);
    template<typename U> static TrueType hasIsAlive(F<sizeof(&U::isAlive)>*);
#endif
    template<typename U> static FalseType hasIsAlive(...);
    template<typename U> static FalseType hasAdjustAndMark(...);

    static bool const value = (sizeof(TrueType) == sizeof(hasAdjustAndMark<T>(0))) && (sizeof(TrueType) == sizeof(hasIsAlive<T>(0)));
};

template <typename T>
struct IsGarbageCollectedType {
    typedef typename WTF::RemoveConst<T>::Type NonConstType;
    typedef WTF::IsSubclassOfTemplate<NonConstType, GarbageCollected> GarbageCollectedSubclass;
    typedef IsGarbageCollectedMixin<NonConstType> GarbageCollectedMixinSubclass;
    typedef WTF::IsSubclassOfTemplate3<NonConstType, HeapHashSet> HeapHashSetSubclass;
    typedef WTF::IsSubclassOfTemplate3<NonConstType, HeapLinkedHashSet> HeapLinkedHashSetSubclass;
    typedef WTF::IsSubclassOfTemplateTypenameSizeTypename<NonConstType, HeapListHashSet> HeapListHashSetSubclass;
    typedef WTF::IsSubclassOfTemplate5<NonConstType, HeapHashMap> HeapHashMapSubclass;
    typedef WTF::IsSubclassOfTemplateTypenameSize<NonConstType, HeapVector> HeapVectorSubclass;
    typedef WTF::IsSubclassOfTemplateTypenameSize<NonConstType, HeapDeque> HeapDequeSubclass;
    typedef WTF::IsSubclassOfTemplate3<NonConstType, HeapHashCountedSet> HeapHashCountedSetSubclass;
    typedef WTF::IsSubclassOfTemplate<NonConstType, HeapTerminatedArray> HeapTerminatedArraySubclass;
    static const bool value =
        GarbageCollectedSubclass::value
        || GarbageCollectedMixinSubclass::value
        || HeapHashSetSubclass::value
        || HeapLinkedHashSetSubclass::value
        || HeapListHashSetSubclass::value
        || HeapHashMapSubclass::value
        || HeapVectorSubclass::value
        || HeapDequeSubclass::value
        || HeapHashCountedSetSubclass::value
        || HeapTerminatedArraySubclass::value;
};

#define COMPILE_ASSERT_IS_GARBAGE_COLLECTED(T, ErrorMessage) \
    COMPILE_ASSERT(IsGarbageCollectedType<T>::value, ErrorMessage)

template<typename T> class Member;

class PersistentNode {
public:
    explicit PersistentNode(TraceCallback trace)
        : m_trace(trace)
    {
    }

    bool isAlive() { return m_trace; }

    virtual ~PersistentNode()
    {
        ASSERT(isAlive());
        m_trace = 0;
    }

    // Ideally the trace method should be virtual and automatically dispatch
    // to the most specific implementation. However having a virtual method
    // on PersistentNode leads to too eager template instantiation with MSVC
    // which leads to include cycles.
    // Instead we call the constructor with a TraceCallback which knows the
    // type of the most specific child and calls trace directly. See
    // TraceMethodDelegate in Visitor.h for how this is done.
    void trace(Visitor* visitor)
    {
        m_trace(visitor, this);
    }

protected:
    TraceCallback m_trace;

private:
    PersistentNode* m_next;
    PersistentNode* m_prev;

    template<typename RootsAccessor, typename Owner> friend class PersistentBase;
    friend class PersistentAnchor;
    friend class ThreadState;
};

// RootsAccessor for Persistent that provides access to thread-local list
// of persistent handles. Can only be used to create handles that
// are constructed and destructed on the same thread.
template<ThreadAffinity Affinity>
class ThreadLocalPersistents {
public:
    static PersistentNode* roots() { return state()->roots(); }

    // No locking required. Just check that we are at the right thread.
    class Lock {
    public:
        Lock() { state()->checkThread(); }
    };

private:
    static ThreadState* state() { return ThreadStateFor<Affinity>::state(); }
};

// RootsAccessor for Persistent that provides synchronized access to global
// list of persistent handles. Can be used for persistent handles that are
// passed between threads.
class GlobalPersistents {
public:
    static PersistentNode* roots() { return ThreadState::globalRoots(); }

    class Lock {
    public:
        Lock() : m_locker(ThreadState::globalRootsMutex()) { }
    private:
        MutexLocker m_locker;
    };
};

// Base class for persistent handles. RootsAccessor specifies which list to
// link resulting handle into. Owner specifies the class containing trace
// method.
template<typename RootsAccessor, typename Owner>
class PersistentBase : public PersistentNode {
public:
    ~PersistentBase()
    {
        typename RootsAccessor::Lock lock;
        ASSERT(m_roots == RootsAccessor::roots()); // Check that the thread is using the same roots list.
        ASSERT(isAlive());
        ASSERT(m_next->isAlive());
        ASSERT(m_prev->isAlive());
        m_next->m_prev = m_prev;
        m_prev->m_next = m_next;
    }

protected:
    inline PersistentBase()
        : PersistentNode(TraceMethodDelegate<Owner, &Owner::trace>::trampoline)
#ifndef NDEBUG
        , m_roots(RootsAccessor::roots())
#endif
    {
        typename RootsAccessor::Lock lock;
        m_prev = RootsAccessor::roots();
        m_next = m_prev->m_next;
        m_prev->m_next = this;
        m_next->m_prev = this;
    }

    inline explicit PersistentBase(const PersistentBase& otherref)
        : PersistentNode(otherref.m_trace)
#ifndef NDEBUG
        , m_roots(RootsAccessor::roots())
#endif
    {
        typename RootsAccessor::Lock lock;
        ASSERT(otherref.m_roots == m_roots); // Handles must belong to the same list.
        PersistentBase* other = const_cast<PersistentBase*>(&otherref);
        m_prev = other;
        m_next = other->m_next;
        other->m_next = this;
        m_next->m_prev = this;
    }

    inline PersistentBase& operator=(const PersistentBase& otherref) { return *this; }

#ifndef NDEBUG
private:
    PersistentNode* m_roots;
#endif
};

// A dummy Persistent handle that ensures the list of persistents is never null.
// This removes a test from a hot path.
class PersistentAnchor : public PersistentNode {
public:
    void trace(Visitor* visitor)
    {
        for (PersistentNode* current = m_next; current != this; current = current->m_next)
            current->trace(visitor);
    }

    virtual ~PersistentAnchor()
    {
        // FIXME: oilpan: Ideally we should have no left-over persistents at this point. However currently there is a
        // large number of objects leaked when we tear down the main thread. Since some of these might contain a
        // persistent or e.g. be RefCountedGarbageCollected we cannot guarantee there are no remaining Persistents at
        // this point.
    }

private:
    PersistentAnchor() : PersistentNode(TraceMethodDelegate<PersistentAnchor, &PersistentAnchor::trace>::trampoline)
    {
        m_next = this;
        m_prev = this;
    }

    friend class ThreadState;
};

#ifndef NDEBUG
    // For global persistent handles we cannot check that the
    // pointer is in the heap because that would involve
    // inspecting the heap of running threads.
#define ASSERT_IS_VALID_PERSISTENT_POINTER(pointer) \
    bool isGlobalPersistent = WTF::IsSubclass<RootsAccessor, GlobalPersistents>::value; \
    ASSERT(!pointer || isGlobalPersistent || ThreadStateFor<ThreadingTrait<T>::Affinity>::state()->contains(pointer))
#else
#define ASSERT_IS_VALID_PERSISTENT_POINTER(pointer)
#endif

template<typename T>
class CrossThreadPersistent;

// Persistent handles are used to store pointers into the
// managed heap. As long as the Persistent handle is alive
// the GC will keep the object pointed to alive. Persistent
// handles can be stored in objects and they are not scoped.
// Persistent handles must not be used to contain pointers
// between objects that are in the managed heap. They are only
// meant to point to managed heap objects from variables/members
// outside the managed heap.
//
// A Persistent is always a GC root from the point of view of
// the garbage collector.
//
// We have to construct and destruct Persistent with default RootsAccessor in
// the same thread.
template<typename T, typename RootsAccessor /* = ThreadLocalPersistents<ThreadingTrait<T>::Affinity > */ >
class Persistent : public PersistentBase<RootsAccessor, Persistent<T, RootsAccessor> > {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(Persistent);
    WTF_DISALLOW_ZERO_ASSIGNMENT(Persistent);
public:
    Persistent() : m_raw(0) { }

    Persistent(std::nullptr_t) : m_raw(0) { }

    Persistent(T* raw) : m_raw(raw)
    {
        ASSERT_IS_VALID_PERSISTENT_POINTER(m_raw);
        recordBacktrace();
    }

    explicit Persistent(T& raw) : m_raw(&raw)
    {
        ASSERT_IS_VALID_PERSISTENT_POINTER(m_raw);
        recordBacktrace();
    }

    Persistent(const Persistent& other) : m_raw(other) { recordBacktrace(); }

    template<typename U>
    Persistent(const Persistent<U, RootsAccessor>& other) : m_raw(other) { recordBacktrace(); }

    template<typename U>
    Persistent(const Member<U>& other) : m_raw(other) { recordBacktrace(); }

    template<typename U>
    Persistent(const RawPtr<U>& other) : m_raw(other.get()) { recordBacktrace(); }

    template<typename U>
    Persistent& operator=(U* other)
    {
        m_raw = other;
        recordBacktrace();
        return *this;
    }

    Persistent& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    void clear() { m_raw = 0; }

    virtual ~Persistent()
    {
        m_raw = 0;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    void trace(Visitor* visitor)
    {
        COMPILE_ASSERT_IS_GARBAGE_COLLECTED(T, NonGarbageCollectedObjectInPersistent);
#if ENABLE(GC_TRACING)
        visitor->setHostInfo(this, m_tracingName.isEmpty() ? "Persistent" : m_tracingName);
#endif
        visitor->mark(m_raw);
    }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    T& operator*() const { return *m_raw; }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }
    operator RawPtr<T>() const { return m_raw; }

    T* operator->() const { return *this; }

    Persistent& operator=(const Persistent& other)
    {
        m_raw = other;
        recordBacktrace();
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Persistent<U, RootsAccessor>& other)
    {
        m_raw = other;
        recordBacktrace();
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Member<U>& other)
    {
        m_raw = other;
        recordBacktrace();
        return *this;
    }

    template<typename U>
    Persistent& operator=(const RawPtr<U>& other)
    {
        m_raw = other;
        recordBacktrace();
        return *this;
    }

    T* get() const { return m_raw; }

private:
#if ENABLE(GC_TRACING)
    void recordBacktrace()
    {
        if (m_raw)
            m_tracingName = Heap::createBacktraceString();
    }

    String m_tracingName;
#else
    inline void recordBacktrace() const { }
#endif
    T* m_raw;

    friend class CrossThreadPersistent<T>;
};

// Unlike Persistent, we can destruct a CrossThreadPersistent in a thread
// different from the construction thread.
template<typename T>
class CrossThreadPersistent : public Persistent<T, GlobalPersistents> {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(CrossThreadPersistent);
    WTF_DISALLOW_ZERO_ASSIGNMENT(CrossThreadPersistent);
public:
    CrossThreadPersistent(T* raw) : Persistent<T, GlobalPersistents>(raw) { }

    using Persistent<T, GlobalPersistents>::operator=;
};

// FIXME: derive affinity based on the collection.
template<typename Collection, ThreadAffinity Affinity = AnyThread>
class PersistentHeapCollectionBase
    : public Collection
    , public PersistentBase<ThreadLocalPersistents<Affinity>, PersistentHeapCollectionBase<Collection, Affinity> > {
    // We overload the various new and delete operators with using the WTF DefaultAllocator to ensure persistent
    // heap collections are always allocated off-heap. This allows persistent collections to be used in
    // DEFINE_STATIC_LOCAL et. al.
    WTF_USE_ALLOCATOR(PersistentHeapCollectionBase, WTF::DefaultAllocator);
public:
    PersistentHeapCollectionBase() { }

    template<typename OtherCollection>
    PersistentHeapCollectionBase(const OtherCollection& other) : Collection(other) { }

    void trace(Visitor* visitor)
    {
#if ENABLE(GC_TRACING)
        visitor->setHostInfo(this, "PersistentHeapCollectionBase");
#endif
        visitor->trace(*static_cast<Collection*>(this));
    }
};

template<
    typename KeyArg,
    typename MappedArg,
    typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>,
    typename MappedTraitsArg = HashTraits<MappedArg> >
class PersistentHeapHashMap : public PersistentHeapCollectionBase<HeapHashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> > { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg> >
class PersistentHeapHashSet : public PersistentHeapCollectionBase<HeapHashSet<ValueArg, HashArg, TraitsArg> > { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg> >
class PersistentHeapLinkedHashSet : public PersistentHeapCollectionBase<HeapLinkedHashSet<ValueArg, HashArg, TraitsArg> > { };

template<
    typename ValueArg,
    size_t inlineCapacity = 0,
    typename HashArg = typename DefaultHash<ValueArg>::Hash>
class PersistentHeapListHashSet : public PersistentHeapCollectionBase<HeapListHashSet<ValueArg, inlineCapacity, HashArg> > { };

template<typename T, typename U, typename V>
class PersistentHeapHashCountedSet : public PersistentHeapCollectionBase<HeapHashCountedSet<T, U, V> > { };

template<typename T, size_t inlineCapacity = 0>
class PersistentHeapVector : public PersistentHeapCollectionBase<HeapVector<T, inlineCapacity> > {
public:
    PersistentHeapVector() { }

    template<size_t otherCapacity>
    PersistentHeapVector(const HeapVector<T, otherCapacity>& other)
        : PersistentHeapCollectionBase<HeapVector<T, inlineCapacity> >(other)
    {
    }
};

template<typename T, size_t inlineCapacity = 0>
class PersistentHeapDeque : public PersistentHeapCollectionBase<HeapDeque<T, inlineCapacity> > {
public:
    PersistentHeapDeque() { }

    template<size_t otherCapacity>
    PersistentHeapDeque(const HeapDeque<T, otherCapacity>& other)
        : PersistentHeapCollectionBase<HeapDeque<T, inlineCapacity> >(other)
    {
    }
};

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template<typename T>
class Member {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(Member);
    WTF_DISALLOW_ZERO_ASSIGNMENT(Member);
public:
    Member() : m_raw(0)
    {
    }

    Member(std::nullptr_t) : m_raw(0)
    {
    }

    Member(T* raw) : m_raw(raw)
    {
    }

    explicit Member(T& raw) : m_raw(&raw)
    {
    }

    template<typename U>
    Member(const RawPtr<U>& other) : m_raw(other.get())
    {
    }

    Member(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1))
    {
    }

    bool isHashTableDeletedValue() const { return m_raw == reinterpret_cast<T*>(-1); }

    template<typename U>
    Member(const Persistent<U>& other) : m_raw(other) { }

    Member(const Member& other) : m_raw(other) { }

    template<typename U>
    Member(const Member<U>& other) : m_raw(other) { }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }

    T* operator->() const { return m_raw; }
    T& operator*() const { return *m_raw; }
    template<typename U>
    operator RawPtr<U>() const { return m_raw; }

    template<typename U>
    Member& operator=(const Persistent<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(const Member<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(U* other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(RawPtr<U> other)
    {
        m_raw = other;
        return *this;
    }

    Member& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    void swap(Member<T>& other) { std::swap(m_raw, other.m_raw); }

    T* get() const { return m_raw; }

    void clear() { m_raw = 0; }


protected:
    void verifyTypeIsGarbageCollected() const
    {
        COMPILE_ASSERT_IS_GARBAGE_COLLECTED(T, NonGarbageCollectedObjectInMember);
    }

    T* m_raw;

    template<bool x, WTF::WeakHandlingFlag y, ShouldWeakPointersBeMarkedStrongly z, typename U, typename V> friend struct CollectionBackingTraceTrait;
    friend class Visitor;
};

template<typename T>
class TraceTrait<Member<T> > {
public:
    static void trace(Visitor* visitor, void* self)
    {
        TraceTrait<T>::mark(visitor, *static_cast<Member<T>*>(self));
    }
};

// TraceTrait to allow compilation of trace method bodies when oilpan is disabled.
// This should never be called, but is needed to compile.
template<typename T>
class TraceTrait<RefPtr<T> > {
public:
    static void trace(Visitor*, void*)
    {
        ASSERT_NOT_REACHED();
    }
};

template<typename T>
class TraceTrait<OwnPtr<T> > {
public:
    static void trace(Visitor* visitor, OwnPtr<T>* ptr)
    {
        TraceTrait<T>::trace(visitor, ptr->get());
    }
};

template<bool needsTracing, typename T>
struct StdPairHelper;

template<typename T>
struct StdPairHelper<false, T>  {
    static void trace(Visitor*, T*) { }
};

template<typename T>
struct StdPairHelper<true, T> {
    static void trace(Visitor* visitor, T* t)
    {
        visitor->trace(*t);
    }
};

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template<typename T, typename U>
class TraceTrait<std::pair<T, U> > {
public:
    static const bool firstNeedsTracing = WTF::NeedsTracing<T>::value || WTF::IsWeak<T>::value;
    static const bool secondNeedsTracing = WTF::NeedsTracing<U>::value || WTF::IsWeak<U>::value;
    static void trace(Visitor* visitor, std::pair<T, U>* pair)
    {
        StdPairHelper<firstNeedsTracing, T>::trace(visitor, &pair->first);
        StdPairHelper<secondNeedsTracing, U>::trace(visitor, &pair->second);
    }
};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember creates
// a weak pointer, which does not keep the pointee alive. Hence if all pointers to
// to a heap allocated object are weak the object will be garbage collected. At the
// time of GC the weak pointers will automatically be set to null.
template<typename T>
class WeakMember : public Member<T> {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(WeakMember);
    WTF_DISALLOW_ZERO_ASSIGNMENT(WeakMember);
public:
    WeakMember() : Member<T>() { }

    WeakMember(std::nullptr_t) : Member<T>(nullptr) { }

    WeakMember(T* raw) : Member<T>(raw) { }

    WeakMember(WTF::HashTableDeletedValueType x) : Member<T>(x) { }

    template<typename U>
    WeakMember(const Persistent<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember(const Member<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember& operator=(const Persistent<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const Member<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(U* other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const RawPtr<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    WeakMember& operator=(std::nullptr_t)
    {
        this->m_raw = 0;
        return *this;
    }

private:
    T** cell() const { return const_cast<T**>(&this->m_raw); }

    friend class Visitor;
};

// Comparison operators between (Weak)Members and Persistents
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }

// CPP-defined type names for the transition period where we want to
// support both reference counting and garbage collection based on a
// compile-time flag.
//
// C++11 template aliases were initially used (with clang only, not
// with GCC nor MSVC.) However, supporting both CPP defines and
// template aliases is problematic from outside a WebCore namespace
// when Oilpan is disabled: e.g.,
// WebCore::RefCountedWillBeGarbageCollected as a template alias would
// uniquely resolve from within any namespace, but if it is backed by
// a CPP #define, it would expand to WebCore::RefCounted, and not the
// required WTF::RefCounted.
//
// Having the CPP expansion instead be fully namespace qualified, and the
// transition type be unqualified, would dually not work for template
// aliases. So, slightly unfortunately, fall back/down to the lowest
// commmon denominator of using CPP macros only.
#if ENABLE(OILPAN)
#define PassRefPtrWillBeRawPtr WTF::RawPtr
#define RefCountedWillBeGarbageCollected WebCore::GarbageCollected
#define RefCountedWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define RefCountedWillBeRefCountedGarbageCollected WebCore::RefCountedGarbageCollected
#define RefCountedGarbageCollectedWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define ThreadSafeRefCountedWillBeGarbageCollected WebCore::GarbageCollected
#define ThreadSafeRefCountedWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define ThreadSafeRefCountedWillBeThreadSafeRefCountedGarbageCollected WebCore::ThreadSafeRefCountedGarbageCollected
#define PersistentWillBeMember WebCore::Member
#define RefPtrWillBePersistent WebCore::Persistent
#define RefPtrWillBeRawPtr WTF::RawPtr
#define RefPtrWillBeMember WebCore::Member
#define RefPtrWillBeWeakMember WebCore::WeakMember
#define RefPtrWillBeCrossThreadPersistent WebCore::CrossThreadPersistent
#define RawPtrWillBeMember WebCore::Member
#define RawPtrWillBeWeakMember WebCore::WeakMember
#define OwnPtrWillBeMember WebCore::Member
#define OwnPtrWillBePersistent WebCore::Persistent
#define OwnPtrWillBeRawPtr WTF::RawPtr
#define PassOwnPtrWillBeRawPtr WTF::RawPtr
#define WeakPtrWillBeMember WebCore::Member
#define WeakPtrWillBeRawPtr WTF::RawPtr
#define WeakPtrWillBeWeakMember WebCore::WeakMember
#define NoBaseWillBeGarbageCollected WebCore::GarbageCollected
#define NoBaseWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define NoBaseWillBeRefCountedGarbageCollected WebCore::RefCountedGarbageCollected
#define WillBeHeapHashMap WebCore::HeapHashMap
#define WillBePersistentHeapHashMap WebCore::PersistentHeapHashMap
#define WillBeHeapHashSet WebCore::HeapHashSet
#define WillBePersistentHeapHashSet WebCore::PersistentHeapHashSet
#define WillBeHeapLinkedHashSet WebCore::HeapLinkedHashSet
#define WillBePersistentHeapLinkedHashSet WebCore::PersistentHeapLinkedHashSet
#define WillBeHeapListHashSet WebCore::HeapListHashSet
#define WillBePersistentHeapListHashSet WebCore::PersistentHeapListHashSet
#define WillBeHeapVector WebCore::HeapVector
#define WillBePersistentHeapVector WebCore::PersistentHeapVector
#define WillBeHeapDeque WebCore::HeapDeque
#define WillBePersistentHeapDeque WebCore::PersistentHeapDeque
#define WillBeHeapHashCountedSet WebCore::HeapHashCountedSet
#define WillBePersistentHeapHashCountedSet WebCore::PersistentHeapHashCountedSet
#define WillBeGarbageCollectedMixin WebCore::GarbageCollectedMixin
#define WillBeHeapSupplement WebCore::HeapSupplement
#define WillBeHeapSupplementable WebCore::HeapSupplementable
#define WillBePersistentHeapSupplementable WebCore::PersistentHeapSupplementable
#define WillBeHeapTerminatedArray WebCore::HeapTerminatedArray
#define WillBeHeapTerminatedArrayBuilder WebCore::HeapTerminatedArrayBuilder
#define WillBeHeapLinkedStack WebCore::HeapLinkedStack
#define PersistentHeapHashSetWillBeHeapHashSet WebCore::HeapHashSet

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr)
{
    static const bool notRefCountedGarbageCollected = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    static const bool notRefCounted = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCounted>::value;
    COMPILE_ASSERT(notRefCountedGarbageCollected, useAdoptRefCountedWillBeRefCountedGarbageCollected);
    COMPILE_ASSERT(notRefCounted, youMustAdopt);
    return PassRefPtrWillBeRawPtr<T>(ptr);
}

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeRefCountedGarbageCollected(T* ptr)
{
    static const bool isRefCountedGarbageCollected = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    COMPILE_ASSERT(isRefCountedGarbageCollected, useAdoptRefWillBeNoop);
    return PassRefPtrWillBeRawPtr<T>(adoptRefCountedGarbageCollected(ptr));
}

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeThreadSafeRefCountedGarbageCollected(T* ptr)
{
    static const bool isThreadSafeRefCountedGarbageCollected = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, ThreadSafeRefCountedGarbageCollected>::value;
    COMPILE_ASSERT(isThreadSafeRefCountedGarbageCollected, useAdoptRefWillBeNoop);
    return PassRefPtrWillBeRawPtr<T>(adoptRefCountedGarbageCollected(ptr));
}

template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr)
{
    static const bool notRefCountedGarbageCollected = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    static const bool notRefCounted = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCounted>::value;
    COMPILE_ASSERT(notRefCountedGarbageCollected, useAdoptRefCountedWillBeRefCountedGarbageCollected);
    COMPILE_ASSERT(notRefCounted, youMustAdopt);
    return PassOwnPtrWillBeRawPtr<T>(ptr);
}

template<typename T> T* adoptPtrWillBeRefCountedGarbageCollected(T* ptr)
{
    static const bool isRefCountedGarbageCollected = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    COMPILE_ASSERT(isRefCountedGarbageCollected, useAdoptRefWillBeNoop);
    return adoptRefCountedGarbageCollected(ptr);
}

template<typename T> T* adoptRefCountedGarbageCollectedWillBeNoop(T* ptr)
{
    static const bool notRefCountedGarbageCollected = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    static const bool notRefCounted = !WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCounted>::value;
    COMPILE_ASSERT(notRefCountedGarbageCollected, useAdoptRefCountedWillBeRefCountedGarbageCollected);
    COMPILE_ASSERT(notRefCounted, youMustAdopt);
    return ptr;
}

#define WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED // do nothing when oilpan is enabled.
#define DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(type) // do nothing
#define DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(type) // do nothing
#define DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(type) // do nothing

#define DEFINE_STATIC_REF_WILL_BE_PERSISTENT(type, name, arguments) \
    DEFINE_STATIC_LOCAL(Persistent<type>, name, arguments)

#else // !ENABLE(OILPAN)

template<typename T>
class DummyBase {
public:
    DummyBase() { }
    ~DummyBase() { }
};

// Export this instance to support WillBeGarbageCollectedMixin
// uses by code residing in non-webcore components.
template class PLATFORM_EXPORT DummyBase<void>;

#define PassRefPtrWillBeRawPtr WTF::PassRefPtr
#define RefCountedWillBeGarbageCollected WTF::RefCounted
#define RefCountedWillBeGarbageCollectedFinalized WTF::RefCounted
#define RefCountedWillBeRefCountedGarbageCollected WTF::RefCounted
#define RefCountedGarbageCollectedWillBeGarbageCollectedFinalized WebCore::RefCountedGarbageCollected
#define ThreadSafeRefCountedWillBeGarbageCollected WTF::ThreadSafeRefCounted
#define ThreadSafeRefCountedWillBeGarbageCollectedFinalized WTF::ThreadSafeRefCounted
#define ThreadSafeRefCountedWillBeThreadSafeRefCountedGarbageCollected WTF::ThreadSafeRefCounted
#define PersistentWillBeMember WebCore::Persistent
#define RefPtrWillBePersistent WTF::RefPtr
#define RefPtrWillBeRawPtr WTF::RefPtr
#define RefPtrWillBeMember WTF::RefPtr
#define RefPtrWillBeWeakMember WTF::RefPtr
#define RefPtrWillBeCrossThreadPersistent WTF::RefPtr
#define RawPtrWillBeMember WTF::RawPtr
#define RawPtrWillBeWeakMember WTF::RawPtr
#define OwnPtrWillBeMember WTF::OwnPtr
#define OwnPtrWillBePersistent WTF::OwnPtr
#define OwnPtrWillBeRawPtr WTF::OwnPtr
#define PassOwnPtrWillBeRawPtr WTF::PassOwnPtr
#define WeakPtrWillBeMember WTF::WeakPtr
#define WeakPtrWillBeRawPtr WTF::WeakPtr
#define WeakPtrWillBeWeakMember WTF::WeakPtr
#define NoBaseWillBeGarbageCollected WebCore::DummyBase
#define NoBaseWillBeGarbageCollectedFinalized WebCore::DummyBase
#define NoBaseWillBeRefCountedGarbageCollected WebCore::DummyBase
#define WillBeHeapHashMap WTF::HashMap
#define WillBePersistentHeapHashMap WTF::HashMap
#define WillBeHeapHashSet WTF::HashSet
#define WillBePersistentHeapHashSet WTF::HashSet
#define WillBeHeapLinkedHashSet WTF::LinkedHashSet
#define WillBePersistentLinkedHeapHashSet WTF::LinkedHashSet
#define WillBeHeapListHashSet WTF::ListHashSet
#define WillBePersistentListHeapHashSet WTF::ListHashSet
#define WillBeHeapVector WTF::Vector
#define WillBePersistentHeapVector WTF::Vector
#define WillBeHeapDeque WTF::Deque
#define WillBePersistentHeapDeque WTF::Deque
#define WillBeHeapHeapCountedSet WTF::HeapCountedSet
#define WillBePersistentHeapHeapCountedSet WTF::HeapCountedSet
#define WillBeGarbageCollectedMixin WebCore::DummyBase<void>
#define WillBeHeapSupplement WebCore::Supplement
#define WillBeHeapSupplementable WebCore::Supplementable
#define WillBePersistentHeapSupplementable WebCore::Supplementable
#define WillBeHeapTerminatedArray WTF::TerminatedArray
#define WillBeHeapTerminatedArrayBuilder WTF::TerminatedArrayBuilder
#define WillBeHeapLinkedStack WTF::LinkedStack
#define PersistentHeapHashSetWillBeHeapHashSet WebCore::PersistentHeapHashSet

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr) { return adoptRef(ptr); }
template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeRefCountedGarbageCollected(T* ptr) { return adoptRef(ptr); }
template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeThreadSafeRefCountedGarbageCollected(T* ptr) { return adoptRef(ptr); }
template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr) { return adoptPtr(ptr); }
template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeRefCountedGarbageCollected(T* ptr) { return adoptPtr(ptr); }

template<typename T> T* adoptRefCountedGarbageCollectedWillBeNoop(T* ptr)
{
    static const bool isRefCountedGarbageCollected = WTF::IsSubclassOfTemplate<typename WTF::RemoveConst<T>::Type, RefCountedGarbageCollected>::value;
    COMPILE_ASSERT(isRefCountedGarbageCollected, useAdoptRefWillBeNoop);
    return adoptRefCountedGarbageCollected(ptr);
}


#define WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED WTF_MAKE_FAST_ALLOCATED
#define DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(type) \
    public:                                            \
        ~type();                                       \
    private:
#define DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(type) \
    public:                                                    \
        virtual ~type();                                       \
    private:

#define DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(type) \
    type::~type() { }

#define DEFINE_STATIC_REF_WILL_BE_PERSISTENT(type, name, arguments) \
    DEFINE_STATIC_REF(type, name, arguments)

#endif // ENABLE(OILPAN)

} // namespace WebCore

namespace WTF {

template <typename T> struct VectorTraits<WebCore::Member<T> > : VectorTraitsBase<WebCore::Member<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<WebCore::WeakMember<T> > : VectorTraitsBase<WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<WebCore::HeapVector<T, 0> > : VectorTraitsBase<WebCore::HeapVector<T, 0> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<WebCore::HeapDeque<T, 0> > : VectorTraitsBase<WebCore::HeapDeque<T, 0> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T, size_t inlineCapacity> struct VectorTraits<WebCore::HeapVector<T, inlineCapacity> > : VectorTraitsBase<WebCore::HeapVector<T, inlineCapacity> > {
    static const bool needsDestruction = VectorTraits<T>::needsDestruction;
    static const bool canInitializeWithMemset = VectorTraits<T>::canInitializeWithMemset;
    static const bool canMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template <typename T, size_t inlineCapacity> struct VectorTraits<WebCore::HeapDeque<T, inlineCapacity> > : VectorTraitsBase<WebCore::HeapDeque<T, inlineCapacity> > {
    static const bool needsDestruction = VectorTraits<T>::needsDestruction;
    static const bool canInitializeWithMemset = VectorTraits<T>::canInitializeWithMemset;
    static const bool canMoveWithMemcpy = VectorTraits<T>::canMoveWithMemcpy;
};

template<typename T> struct HashTraits<WebCore::Member<T> > : SimpleClassHashTraits<WebCore::Member<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef RawPtr<T> PeekInType;
    typedef RawPtr<T> PassInType;
    typedef WebCore::Member<T>* IteratorGetType;
    typedef const WebCore::Member<T>* IteratorConstGetType;
    typedef WebCore::Member<T>& IteratorReferenceType;
    typedef T* const IteratorConstReferenceType;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x->get(); }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::Member<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::Member<T>& value) { return value; }
    static PassOutType passOut(const WebCore::Member<T>& value) { return value; }
};

template<typename T> struct HashTraits<WebCore::WeakMember<T> > : SimpleClassHashTraits<WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef RawPtr<T> PeekInType;
    typedef RawPtr<T> PassInType;
    typedef WebCore::WeakMember<T>* IteratorGetType;
    typedef const WebCore::WeakMember<T>* IteratorConstGetType;
    typedef WebCore::WeakMember<T>& IteratorReferenceType;
    typedef T* const IteratorConstReferenceType;
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return *x; }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x->get(); }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::WeakMember<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::WeakMember<T>& value) { return value; }
    static PassOutType passOut(const WebCore::WeakMember<T>& value) { return value; }
    static bool shouldRemoveFromCollection(WebCore::Visitor* visitor, WebCore::WeakMember<T>& value) { return !visitor->isAlive(value); }
    static void traceInCollection(WebCore::Visitor* visitor, WebCore::WeakMember<T>& weakMember, WebCore::ShouldWeakPointersBeMarkedStrongly strongify)
    {
        if (strongify == WebCore::WeakPointersActStrong)
            visitor->trace(reinterpret_cast<WebCore::Member<T>&>(weakMember)); // Strongified visit.
    }
};

template<typename T> struct PtrHash<WebCore::Member<T> > : PtrHash<T*> {
    template<typename U>
    static unsigned hash(const U& key) { return PtrHash<T*>::hash(key); }
    static bool equal(T* a, const WebCore::Member<T>& b) { return a == b; }
    static bool equal(const WebCore::Member<T>& a, T* b) { return a == b; }
    template<typename U, typename V>
    static bool equal(const U& a, const V& b) { return a == b; }
};

template<typename T> struct PtrHash<WebCore::WeakMember<T> > : PtrHash<WebCore::Member<T> > {
};

template<typename P> struct PtrHash<WebCore::Persistent<P> > : PtrHash<P*> {
    using PtrHash<P*>::hash;
    static unsigned hash(const RefPtr<P>& key) { return hash(key.get()); }
    using PtrHash<P*>::equal;
    static bool equal(const RefPtr<P>& a, const RefPtr<P>& b) { return a == b; }
    static bool equal(P* a, const RefPtr<P>& b) { return a == b; }
    static bool equal(const RefPtr<P>& a, P* b) { return a == b; }
};

// PtrHash is the default hash for hash tables with members.
template<typename T> struct DefaultHash<WebCore::Member<T> > {
    typedef PtrHash<WebCore::Member<T> > Hash;
};

template<typename T> struct DefaultHash<WebCore::WeakMember<T> > {
    typedef PtrHash<WebCore::WeakMember<T> > Hash;
};

template<typename T> struct DefaultHash<WebCore::Persistent<T> > {
    typedef PtrHash<WebCore::Persistent<T> > Hash;
};

template<typename T>
struct NeedsTracing<WebCore::Member<T> > {
    static const bool value = true;
};

template<typename T>
struct IsWeak<WebCore::WeakMember<T> > {
    static const bool value = true;
};

template<typename T> inline T* getPtr(const WebCore::Member<T>& p)
{
    return p.get();
}

template<typename T, typename U>
struct NeedsTracing<std::pair<T, U> > {
    static const bool value = NeedsTracing<T>::value || NeedsTracing<U>::value || IsWeak<T>::value || IsWeak<U>::value;
};

template<typename T>
struct NeedsTracing<OwnPtr<T> > {
    static const bool value = NeedsTracing<T>::value;
};

// We define specialization of the NeedsTracing trait for off heap collections
// since we don't support tracing them.
template<typename T, size_t N>
struct NeedsTracing<Vector<T, N> > {
    static const bool value = false;
};

template<typename T, size_t N>
struct NeedsTracing<Deque<T, N> > {
    static const bool value = false;
};

template<typename T, typename U, typename V>
struct NeedsTracing<HashCountedSet<T, U, V> > {
    static const bool value = false;
};

template<typename T, typename U, typename V>
struct NeedsTracing<HashSet<T, U, V> > {
    static const bool value = false;
};

template<typename T, size_t U, typename V>
struct NeedsTracing<ListHashSet<T, U, V> > {
    static const bool value = false;
};

template<typename T, typename U, typename V>
struct NeedsTracing<LinkedHashSet<T, U, V> > {
    static const bool value = false;
};

template<typename T, typename U, typename V, typename W, typename X>
struct NeedsTracing<HashMap<T, U, V, W, X> > {
    static const bool value = false;
};

template<typename T, size_t inlineCapacity>
struct NeedsTracing<ListHashSetNode<T, WebCore::HeapListHashSetAllocator<T, inlineCapacity> > *> {
    // All heap allocated node pointers need visiting to keep the nodes alive,
    // regardless of whether they contain pointers to other heap allocated
    // objects.
    static const bool value = true;
};

// For wtf/Functional.h
template<typename T, bool isGarbageCollected> struct PointerParamStorageTraits;

template<typename T>
struct PointerParamStorageTraits<T*, false> {
    typedef T* StorageType;

    static StorageType wrap(T* value) { return value; }
    static T* unwrap(const StorageType& value) { return value; }
};

template<typename T>
struct PointerParamStorageTraits<T*, true> {
    typedef WebCore::CrossThreadPersistent<T> StorageType;

    static StorageType wrap(T* value) { return value; }
    static T* unwrap(const StorageType& value) { return value.get(); }
};

template<typename T>
struct ParamStorageTraits<T*> : public PointerParamStorageTraits<T*, WebCore::IsGarbageCollectedType<T>::value> {
};

template<typename T>
struct ParamStorageTraits<RawPtr<T> > : public PointerParamStorageTraits<T*, WebCore::IsGarbageCollectedType<T>::value> {
};

} // namespace WTF

#endif
