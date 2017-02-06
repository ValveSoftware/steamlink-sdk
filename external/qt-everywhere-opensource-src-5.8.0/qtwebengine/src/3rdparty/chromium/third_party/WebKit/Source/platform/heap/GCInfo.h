// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCInfo_h
#define GCInfo_h

#include "platform/heap/Visitor.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/Deque.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/HashTable.h"
#include "wtf/LinkedHashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/TypeTraits.h"
#include "wtf/Vector.h"

namespace blink {

// The FinalizerTraitImpl specifies how to finalize objects. Object
// that inherit from GarbageCollectedFinalized are finalized by
// calling their 'finalize' method which by default will call the
// destructor on the object.
template<typename T, bool isGarbageCollectedFinalized>
struct FinalizerTraitImpl;

template<typename T>
struct FinalizerTraitImpl<T, true> {
    STATIC_ONLY(FinalizerTraitImpl);
    static void finalize(void* obj)
    {
        static_assert(sizeof(T), "T must be fully defined");
        static_cast<T*>(obj)->finalizeGarbageCollectedObject();
    };
};

template<typename T>
struct FinalizerTraitImpl<T, false> {
    STATIC_ONLY(FinalizerTraitImpl);
    static void finalize(void* obj)
    {
        static_assert(sizeof(T), "T must be fully defined");
    };
};

// The FinalizerTrait is used to determine if a type requires
// finalization and what finalization means.
//
// By default classes that inherit from GarbageCollectedFinalized need
// finalization and finalization means calling the 'finalize' method
// of the object. The FinalizerTrait can be specialized if the default
// behavior is not desired.
template<typename T>
struct FinalizerTrait {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type, GarbageCollectedFinalized>::value;
    static void finalize(void* obj) { FinalizerTraitImpl<T, nonTrivialFinalizer>::finalize(obj); }
};

class HeapAllocator;
template<typename ValueArg, size_t inlineCapacity> class HeapListHashSetAllocator;
template<typename T, typename Traits> class HeapVectorBacking;
template<typename Table> class HeapHashTableBacking;

template<typename T, typename U, typename V>
struct FinalizerTrait<LinkedHashSet<T, U, V, HeapAllocator>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = true;
    static void finalize(void* obj) { FinalizerTraitImpl<LinkedHashSet<T, U, V, HeapAllocator>, nonTrivialFinalizer>::finalize(obj); }
};

template<typename T, typename Allocator>
struct FinalizerTrait<WTF::ListHashSetNode<T, Allocator>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = !WTF::IsTriviallyDestructible<T>::value;
    static void finalize(void* obj) { FinalizerTraitImpl<WTF::ListHashSetNode<T, Allocator>, nonTrivialFinalizer>::finalize(obj); }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = inlineCapacity && VectorTraits<T>::needsDestruction;
    static void finalize(void* obj) { FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>, nonTrivialFinalizer>::finalize(obj); }
};

template<typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = inlineCapacity && VectorTraits<T>::needsDestruction;
    static void finalize(void* obj) { FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>, nonTrivialFinalizer>::finalize(obj); }
};

template<typename Table>
struct FinalizerTrait<HeapHashTableBacking<Table>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = !WTF::IsTriviallyDestructible<typename Table::ValueType>::value;
    static void finalize(void* obj) { FinalizerTraitImpl<HeapHashTableBacking<Table>, nonTrivialFinalizer>::finalize(obj); }
};

template<typename T, typename Traits>
struct FinalizerTrait<HeapVectorBacking<T, Traits>> {
    STATIC_ONLY(FinalizerTrait);
    static const bool nonTrivialFinalizer = Traits::needsDestruction;
    static void finalize(void* obj) { FinalizerTraitImpl<HeapVectorBacking<T, Traits>, nonTrivialFinalizer>::finalize(obj); }
};

// s_gcInfoTable holds the per-class GCInfo descriptors; each heap
// object header keeps its index into this table.
extern PLATFORM_EXPORT GCInfo const** s_gcInfoTable;

// GCInfo contains meta-data associated with objects allocated in the
// Blink heap. This meta-data consists of a function pointer used to
// trace the pointers in the object during garbage collection, an
// indication of whether or not the object needs a finalization
// callback, and a function pointer used to finalize the object when
// the garbage collector determines that the object is no longer
// reachable. There is a GCInfo struct for each class that directly
// inherits from GarbageCollected or GarbageCollectedFinalized.
struct GCInfo {
    bool hasFinalizer() const { return m_nonTrivialFinalizer; }
    bool hasVTable() const { return m_hasVTable; }
    TraceCallback m_trace;
    FinalizationCallback m_finalize;
    bool m_nonTrivialFinalizer;
    bool m_hasVTable;
};

#if ENABLE(ASSERT)
PLATFORM_EXPORT void assertObjectHasGCInfo(const void*, size_t gcInfoIndex);
#endif

class GCInfoTable {
    STATIC_ONLY(GCInfoTable);
public:
    PLATFORM_EXPORT static void ensureGCInfoIndex(const GCInfo*, size_t*);

    static void init();
    static void shutdown();

    static size_t gcInfoIndex() { return s_gcInfoIndex; }

    // The (max + 1) GCInfo index supported.
    // We assume that 14 bits is enough to represent all possible types: during
    // telemetry runs, we see about 1000 different types, looking at the output
    // of the oilpan gc clang plugin, there appear to be at most about 6000
    // types, so 14 bits should be more than twice as many bits as we will ever
    // encounter.
    static const size_t maxIndex = 1 << 14;

private:
    static void resize();

    static int s_gcInfoIndex;
    static size_t s_gcInfoTableSize;
};

// GCInfotAtBaseType should be used when returning a unique 14 bit integer
// for a given gcInfo.
template<typename T>
struct GCInfoAtBaseType {
    STATIC_ONLY(GCInfoAtBaseType);
    static size_t index()
    {
        static_assert(sizeof(T), "T must be fully defined");
        static const GCInfo gcInfo = {
            TraceTrait<T>::trace,
            FinalizerTrait<T>::finalize,
            FinalizerTrait<T>::nonTrivialFinalizer,
            std::is_polymorphic<T>::value,
        };

        static size_t gcInfoIndex = 0;
        ASSERT(s_gcInfoTable);
        if (!acquireLoad(&gcInfoIndex))
            GCInfoTable::ensureGCInfoIndex(&gcInfo, &gcInfoIndex);
        ASSERT(gcInfoIndex >= 1);
        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        return gcInfoIndex;
    }
};

template<typename T, bool = WTF::IsSubclassOfTemplate<typename std::remove_const<T>::type, GarbageCollected>::value> struct GetGarbageCollectedType;

template<typename T>
struct GetGarbageCollectedType<T, true> {
    STATIC_ONLY(GetGarbageCollectedType);
    using type = typename T::GarbageCollectedType;
};

template<typename T>
struct GetGarbageCollectedType<T, false> {
    STATIC_ONLY(GetGarbageCollectedType);
    using type = T;
};

template<typename T>
struct GCInfoTrait {
    STATIC_ONLY(GCInfoTrait);
    static size_t index()
    {
        return GCInfoAtBaseType<typename GetGarbageCollectedType<T>::type>::index();
    }
};

template<typename U> class GCInfoTrait<const U> : public GCInfoTrait<U> { };

template<typename T, typename U, typename V, typename W, typename X> class HeapHashMap;
template<typename T, typename U, typename V> class HeapHashSet;
template<typename T, typename U, typename V> class HeapLinkedHashSet;
template<typename T, size_t inlineCapacity, typename U> class HeapListHashSet;
template<typename T, size_t inlineCapacity> class HeapVector;
template<typename T, size_t inlineCapacity> class HeapDeque;
template<typename T, typename U, typename V> class HeapHashCountedSet;

template<typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X>> : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V>> : public GCInfoTrait<HashSet<T, U, V, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapLinkedHashSet<T, U, V>> : public GCInfoTrait<LinkedHashSet<T, U, V, HeapAllocator>> { };
template<typename T, size_t inlineCapacity, typename U>
struct GCInfoTrait<HeapListHashSet<T, inlineCapacity, U>> : public GCInfoTrait<ListHashSet<T, inlineCapacity, U, HeapListHashSetAllocator<T, inlineCapacity>>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity>> : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> { };
template<typename T, size_t inlineCapacity>
struct GCInfoTrait<HeapDeque<T, inlineCapacity>> : public GCInfoTrait<Deque<T, inlineCapacity, HeapAllocator>> { };
template<typename T, typename U, typename V>
struct GCInfoTrait<HeapHashCountedSet<T, U, V>> : public GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> { };

} // namespace blink

#endif
