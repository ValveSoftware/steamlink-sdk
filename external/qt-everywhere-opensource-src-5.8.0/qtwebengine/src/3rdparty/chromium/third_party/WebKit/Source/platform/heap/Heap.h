/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef Heap_h
#define Heap_h

#include "platform/PlatformExport.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT HeapAllocHooks {
public:
    // TODO(hajimehoshi): Pass a type name of the allocated object.
    typedef void AllocationHook(Address, size_t, const char*);
    typedef void FreeHook(Address);

    static void setAllocationHook(AllocationHook* hook) { m_allocationHook = hook; }
    static void setFreeHook(FreeHook* hook) { m_freeHook = hook; }

    static void allocationHookIfEnabled(Address address, size_t size, const char* typeName)
    {
        AllocationHook* allocationHook = m_allocationHook;
        if (UNLIKELY(!!allocationHook))
            allocationHook(address, size, typeName);
    }

    static void freeHookIfEnabled(Address address)
    {
        FreeHook* freeHook = m_freeHook;
        if (UNLIKELY(!!freeHook))
            freeHook(address);
    }

private:
    static AllocationHook* m_allocationHook;
    static FreeHook* m_freeHook;
};

class CrossThreadPersistentRegion;
template<typename T> class Member;
template<typename T> class WeakMember;
template<typename T> class UntracedMember;

template<typename T, bool = NeedsAdjustAndMark<T>::value> class ObjectAliveTrait;

template<typename T>
class ObjectAliveTrait<T, false> {
    STATIC_ONLY(ObjectAliveTrait);
public:
    static bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        return HeapObjectHeader::fromPayload(object)->isMarked();
    }
};

template<typename T>
class ObjectAliveTrait<T, true> {
    STATIC_ONLY(ObjectAliveTrait);
public:
    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    static bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        return object->isHeapObjectAlive();
    }
};

class PLATFORM_EXPORT ProcessHeap {
    STATIC_ONLY(ProcessHeap);
public:
    static void init();
    static void shutdown();

    static CrossThreadPersistentRegion& crossThreadPersistentRegion();

    static bool isLowEndDevice() { return s_isLowEndDevice; }
    static void increaseTotalAllocatedObjectSize(size_t delta) { atomicAdd(&s_totalAllocatedObjectSize, static_cast<long>(delta)); }
    static void decreaseTotalAllocatedObjectSize(size_t delta) { atomicSubtract(&s_totalAllocatedObjectSize, static_cast<long>(delta)); }
    static size_t totalAllocatedObjectSize() { return acquireLoad(&s_totalAllocatedObjectSize); }
    static void increaseTotalMarkedObjectSize(size_t delta) { atomicAdd(&s_totalMarkedObjectSize, static_cast<long>(delta)); }
    static void decreaseTotalMarkedObjectSize(size_t delta) { atomicSubtract(&s_totalMarkedObjectSize, static_cast<long>(delta)); }
    static size_t totalMarkedObjectSize() { return acquireLoad(&s_totalMarkedObjectSize); }
    static void increaseTotalAllocatedSpace(size_t delta) { atomicAdd(&s_totalAllocatedSpace, static_cast<long>(delta)); }
    static void decreaseTotalAllocatedSpace(size_t delta) { atomicSubtract(&s_totalAllocatedSpace, static_cast<long>(delta)); }
    static size_t totalAllocatedSpace() { return acquireLoad(&s_totalAllocatedSpace); }
    static void resetHeapCounters();

private:
    static bool s_shutdownComplete;
    static bool s_isLowEndDevice;
    static size_t s_totalAllocatedSpace;
    static size_t s_totalAllocatedObjectSize;
    static size_t s_totalMarkedObjectSize;

    friend class ThreadState;
};

// Stats for the heap.
class ThreadHeapStats {
    USING_FAST_MALLOC(ThreadHeapStats);
public:
    ThreadHeapStats();
    void setMarkedObjectSizeAtLastCompleteSweep(size_t size) { releaseStore(&m_markedObjectSizeAtLastCompleteSweep, size); }
    size_t markedObjectSizeAtLastCompleteSweep() { return acquireLoad(&m_markedObjectSizeAtLastCompleteSweep); }
    void increaseAllocatedObjectSize(size_t delta);
    void decreaseAllocatedObjectSize(size_t delta);
    size_t allocatedObjectSize() { return acquireLoad(&m_allocatedObjectSize); }
    void increaseMarkedObjectSize(size_t delta);
    size_t markedObjectSize() { return acquireLoad(&m_markedObjectSize); }
    void increaseAllocatedSpace(size_t delta);
    void decreaseAllocatedSpace(size_t delta);
    size_t allocatedSpace() { return acquireLoad(&m_allocatedSpace); }
    size_t objectSizeAtLastGC() { return acquireLoad(&m_objectSizeAtLastGC); }
    void increaseWrapperCount(size_t delta) { atomicAdd(&m_wrapperCount, static_cast<long>(delta)); }
    void decreaseWrapperCount(size_t delta) { atomicSubtract(&m_wrapperCount, static_cast<long>(delta)); }
    size_t wrapperCount() { return acquireLoad(&m_wrapperCount); }
    size_t wrapperCountAtLastGC() { return acquireLoad(&m_wrapperCountAtLastGC); }
    void increaseCollectedWrapperCount(size_t delta) { atomicAdd(&m_collectedWrapperCount, static_cast<long>(delta)); }
    size_t collectedWrapperCount() { return acquireLoad(&m_collectedWrapperCount); }
    size_t partitionAllocSizeAtLastGC() { return acquireLoad(&m_partitionAllocSizeAtLastGC); }
    void setEstimatedMarkingTimePerByte(double estimatedMarkingTimePerByte) { m_estimatedMarkingTimePerByte = estimatedMarkingTimePerByte; }
    double estimatedMarkingTimePerByte() const { return m_estimatedMarkingTimePerByte; }
    double estimatedMarkingTime();
    void reset();

private:
    size_t m_allocatedSpace;
    size_t m_allocatedObjectSize;
    size_t m_objectSizeAtLastGC;
    size_t m_markedObjectSize;
    size_t m_markedObjectSizeAtLastCompleteSweep;
    size_t m_wrapperCount;
    size_t m_wrapperCountAtLastGC;
    size_t m_collectedWrapperCount;
    size_t m_partitionAllocSizeAtLastGC;
    double m_estimatedMarkingTimePerByte;
};

using ThreadStateSet = HashSet<ThreadState*>;

class PLATFORM_EXPORT ThreadHeap {
public:
    ThreadHeap();
    ~ThreadHeap();

    // Returns true for main thread's heap.
    // TODO(keishi): Per-thread-heap will return false.
    bool isMainThreadHeap() { return this == ThreadHeap::mainThreadHeap(); }
    static ThreadHeap* mainThreadHeap() { return s_mainThreadHeap; }

#if ENABLE(ASSERT)
    bool isAtSafePoint();
    BasePage* findPageFromAddress(Address);
#endif

    template<typename T>
    static inline bool isHeapObjectAlive(T* object)
    {
        static_assert(sizeof(T), "T must be fully defined");
        // The strongification of collections relies on the fact that once a
        // collection has been strongified, there is no way that it can contain
        // non-live entries, so no entries will be removed. Since you can't set
        // the mark bit on a null pointer, that means that null pointers are
        // always 'alive'.
        if (!object)
            return true;
        // TODO(keishi): some tests create CrossThreadPersistent on non attached threads.
        if (!ThreadState::current())
            return true;
        if (&ThreadState::current()->heap() != &pageFromObject(object)->arena()->getThreadState()->heap())
            return true;
        return ObjectAliveTrait<T>::isHeapObjectAlive(object);
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const Member<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const WeakMember<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const UntracedMember<T>& member)
    {
        return isHeapObjectAlive(member.get());
    }
    template<typename T>
    static inline bool isHeapObjectAlive(const T*& ptr)
    {
        return isHeapObjectAlive(ptr);
    }

    RecursiveMutex& threadAttachMutex() { return m_threadAttachMutex; }
    const ThreadStateSet& threads() const { return m_threads; }
    ThreadHeapStats& heapStats() { return m_stats; }
    SafePointBarrier* safePointBarrier() { return m_safePointBarrier.get(); }
    CallbackStack* markingStack() const { return m_markingStack.get(); }
    CallbackStack* postMarkingCallbackStack() const { return m_postMarkingCallbackStack.get(); }
    CallbackStack* globalWeakCallbackStack() const { return m_globalWeakCallbackStack.get(); }
    CallbackStack* ephemeronStack() const { return m_ephemeronStack.get(); }

    void attach(ThreadState*);
    void detach(ThreadState*);
    void lockThreadAttachMutex();
    void unlockThreadAttachMutex();
    bool park();
    void resume();

    void visitPersistentRoots(Visitor*);
    void visitStackRoots(Visitor*);
    void checkAndPark(ThreadState*, SafePointAwareMutexLocker*);
    void enterSafePoint(ThreadState*);
    void leaveSafePoint(ThreadState*, SafePointAwareMutexLocker*);

    // Add a weak pointer callback to the weak callback work list.  General
    // object pointer callbacks are added to a thread local weak callback work
    // list and the callback is called on the thread that owns the object, with
    // the closure pointer as an argument.  Most of the time, the closure and
    // the containerObject can be the same thing, but the containerObject is
    // constrained to be on the heap, since the heap is used to identify the
    // correct thread.
    void pushThreadLocalWeakCallback(void* closure, void* containerObject, WeakCallback);

    static RecursiveMutex& allHeapsMutex();
    static HashSet<ThreadHeap*>& allHeaps();

    // Is the finalizable GC object still alive, but slated for lazy sweeping?
    // If a lazy sweep is in progress, returns true if the object was found
    // to be not reachable during the marking phase, but it has yet to be swept
    // and finalized. The predicate returns false in all other cases.
    //
    // Holding a reference to an already-dead object is not a valid state
    // to be in; willObjectBeLazilySwept() has undefined behavior if passed
    // such a reference.
    template<typename T>
    NO_LAZY_SWEEP_SANITIZE_ADDRESS
    static bool willObjectBeLazilySwept(const T* objectPointer)
    {
        static_assert(IsGarbageCollectedType<T>::value, "only objects deriving from GarbageCollected can be used.");
        BasePage* page = pageFromObject(objectPointer);
        // Page has been swept and it is still alive.
        if (page->hasBeenSwept())
            return false;
        ASSERT(page->arena()->getThreadState()->isSweepingInProgress());

        // If marked and alive, the object hasn't yet been swept..and won't
        // be once its page is processed.
        if (ThreadHeap::isHeapObjectAlive(const_cast<T*>(objectPointer)))
            return false;

        if (page->isLargeObjectPage())
            return true;

        // If the object is unmarked, it may be on the page currently being
        // lazily swept.
        return page->arena()->willObjectBeLazilySwept(page, const_cast<T*>(objectPointer));
    }

    // Push a trace callback on the marking stack.
    void pushTraceCallback(void* containerObject, TraceCallback);

    // Push a trace callback on the post-marking callback stack.  These
    // callbacks are called after normal marking (including ephemeron
    // iteration).
    void pushPostMarkingCallback(void*, TraceCallback);

    // Similar to the more general pushThreadLocalWeakCallback, but cell
    // pointer callbacks are added to a static callback work list and the weak
    // callback is performed on the thread performing garbage collection.  This
    // is OK because cells are just cleared and no deallocation can happen.
    void pushGlobalWeakCallback(void** cell, WeakCallback);

    // Pop the top of a marking stack and call the callback with the visitor
    // and the object.  Returns false when there is nothing more to do.
    bool popAndInvokeTraceCallback(Visitor*);

    // Remove an item from the post-marking callback stack and call
    // the callback with the visitor and the object pointer.  Returns
    // false when there is nothing more to do.
    bool popAndInvokePostMarkingCallback(Visitor*);

    // Remove an item from the weak callback work list and call the callback
    // with the visitor and the closure pointer.  Returns false when there is
    // nothing more to do.
    bool popAndInvokeGlobalWeakCallback(Visitor*);

    // Register an ephemeron table for fixed-point iteration.
    void registerWeakTable(void* containerObject, EphemeronCallback, EphemeronCallback);
#if ENABLE(ASSERT)
    bool weakTableRegistered(const void*);
#endif

    BlinkGC::GCReason lastGCReason() { return m_lastGCReason; }
    RegionTree* getRegionTree() { return m_regionTree.get(); }

    static inline size_t allocationSizeFromSize(size_t size)
    {
        // Add space for header.
        size_t allocationSize = size + sizeof(HeapObjectHeader);
        // The allocation size calculation can overflow for large sizes.
        RELEASE_ASSERT(allocationSize > size);
        // Align size with allocation granularity.
        allocationSize = (allocationSize + allocationMask) & ~allocationMask;
        return allocationSize;
    }
    static Address allocateOnArenaIndex(ThreadState*, size_t, int arenaIndex, size_t gcInfoIndex, const char* typeName);
    template<typename T> static Address allocate(size_t, bool eagerlySweep = false);
    template<typename T> static Address reallocate(void* previous, size_t);

    static const char* gcReasonString(BlinkGC::GCReason);
    static void collectGarbage(BlinkGC::StackState, BlinkGC::GCType, BlinkGC::GCReason);
    static void collectGarbageForTerminatingThread(ThreadState*);
    static void collectAllGarbage();

    void processMarkingStack(Visitor*);
    void postMarkingProcessing(Visitor*);
    void globalWeakProcessing(Visitor*);

    void preGC();
    void postGC(BlinkGC::GCType);

    // Conservatively checks whether an address is a pointer in any of the
    // thread heaps.  If so marks the object pointed to as live.
    Address checkAndMarkPointer(Visitor*, Address);

    size_t objectPayloadSizeForTesting();

    void flushHeapDoesNotContainCache();

    FreePagePool* getFreePagePool() { return m_freePagePool.get(); }
    OrphanedPagePool* getOrphanedPagePool() { return m_orphanedPagePool.get(); }

    // This look-up uses the region search tree and a negative contains cache to
    // provide an efficient mapping from arbitrary addresses to the containing
    // heap-page if one exists.
    BasePage* lookupPageForAddress(Address);

    static const GCInfo* gcInfo(size_t gcInfoIndex)
    {
        ASSERT(gcInfoIndex >= 1);
        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        ASSERT(s_gcInfoTable);
        const GCInfo* info = s_gcInfoTable[gcInfoIndex];
        ASSERT(info);
        return info;
    }

    static void reportMemoryUsageHistogram();
    static void reportMemoryUsageForTracing();

private:
    // Reset counters that track live and allocated-since-last-GC sizes.
    void resetHeapCounters();

    static int arenaIndexForObjectSize(size_t);
    static bool isNormalArenaIndex(int);

    void decommitCallbackStacks();

    RecursiveMutex m_threadAttachMutex;
    ThreadStateSet m_threads;
    ThreadHeapStats m_stats;
    std::unique_ptr<RegionTree> m_regionTree;
    std::unique_ptr<HeapDoesNotContainCache> m_heapDoesNotContainCache;
    std::unique_ptr<SafePointBarrier> m_safePointBarrier;
    std::unique_ptr<FreePagePool> m_freePagePool;
    std::unique_ptr<OrphanedPagePool> m_orphanedPagePool;
    std::unique_ptr<CallbackStack> m_markingStack;
    std::unique_ptr<CallbackStack> m_postMarkingCallbackStack;
    std::unique_ptr<CallbackStack> m_globalWeakCallbackStack;
    std::unique_ptr<CallbackStack> m_ephemeronStack;
    BlinkGC::GCReason m_lastGCReason;

    static ThreadHeap* s_mainThreadHeap;

    friend class ThreadState;
};

template<typename T>
struct IsEagerlyFinalizedType {
    STATIC_ONLY(IsEagerlyFinalizedType);
private:
    typedef char YesType;
    struct NoType {
        char padding[8];
    };

    template <typename U> static YesType checkMarker(typename U::IsEagerlyFinalizedMarker*);
    template <typename U> static NoType checkMarker(...);

public:
    static const bool value = sizeof(checkMarker<T>(nullptr)) == sizeof(YesType);
};

template<typename T> class GarbageCollected {
    IS_GARBAGE_COLLECTED_TYPE();
    WTF_MAKE_NONCOPYABLE(GarbageCollected);

    // For now direct allocation of arrays on the heap is not allowed.
    void* operator new[](size_t size);

#if OS(WIN) && COMPILER(MSVC)
    // Due to some quirkiness in the MSVC compiler we have to provide
    // the delete[] operator in the GarbageCollected subclasses as it
    // is called when a class is exported in a DLL.
protected:
    void operator delete[](void* p)
    {
        ASSERT_NOT_REACHED();
    }
#else
    void operator delete[](void* p);
#endif

public:
    using GarbageCollectedType = T;

    void* operator new(size_t size)
    {
        return allocateObject(size, IsEagerlyFinalizedType<T>::value);
    }

    static void* allocateObject(size_t size, bool eagerlySweep)
    {
        return ThreadHeap::allocate<T>(size, eagerlySweep);
    }

    void operator delete(void* p)
    {
        ASSERT_NOT_REACHED();
    }

protected:
    GarbageCollected()
    {
    }
};

// Assigning class types to their arenas.
//
// We use sized arenas for most 'normal' objects to improve memory locality.
// It seems that the same type of objects are likely to be accessed together,
// which means that we want to group objects by type. That's one reason
// why we provide dedicated arenas for popular types (e.g., Node, CSSValue),
// but it's not practical to prepare dedicated arenas for all types.
// Thus we group objects by their sizes, hoping that this will approximately
// group objects by their types.
//
// An exception to the use of sized arenas is made for class types that
// require prompt finalization after a garbage collection. That is, their
// instances have to be finalized early and cannot be delayed until lazy
// sweeping kicks in for their heap and page. The EAGERLY_FINALIZE()
// macro is used to declare a class (and its derived classes) as being
// in need of eager finalization. Must be defined with 'public' visibility
// for a class.
//

inline int ThreadHeap::arenaIndexForObjectSize(size_t size)
{
    if (size < 64) {
        if (size < 32)
            return BlinkGC::NormalPage1ArenaIndex;
        return BlinkGC::NormalPage2ArenaIndex;
    }
    if (size < 128)
        return BlinkGC::NormalPage3ArenaIndex;
    return BlinkGC::NormalPage4ArenaIndex;
}

inline bool ThreadHeap::isNormalArenaIndex(int index)
{
    return index >= BlinkGC::NormalPage1ArenaIndex && index <= BlinkGC::NormalPage4ArenaIndex;
}

#define DECLARE_EAGER_FINALIZATION_OPERATOR_NEW() \
public:                                           \
    GC_PLUGIN_IGNORE("491488")                    \
    void* operator new(size_t size)               \
    {                                             \
        return allocateObject(size, true);        \
    }

#define IS_EAGERLY_FINALIZED() (pageFromObject(this)->arena()->arenaIndex() == BlinkGC::EagerSweepArenaIndex)
#if ENABLE(ASSERT)
class VerifyEagerFinalization {
    DISALLOW_NEW();
public:
    ~VerifyEagerFinalization()
    {
        // If this assert triggers, the class annotated as eagerly
        // finalized ended up not being allocated on the heap
        // set aside for eager finalization. The reason is most
        // likely that the effective 'operator new' overload for
        // this class' leftmost base is for a class that is not
        // eagerly finalized. Declaring and defining an 'operator new'
        // for this class is what's required -- consider using
        // DECLARE_EAGER_FINALIZATION_OPERATOR_NEW().
        ASSERT(IS_EAGERLY_FINALIZED());
    }
};
#define EAGERLY_FINALIZE()                             \
private:                                               \
    VerifyEagerFinalization m_verifyEagerFinalization; \
public:                                                \
    typedef int IsEagerlyFinalizedMarker
#else
#define EAGERLY_FINALIZE()                             \
public:                                                \
    typedef int IsEagerlyFinalizedMarker
#endif

inline Address ThreadHeap::allocateOnArenaIndex(ThreadState* state, size_t size, int arenaIndex, size_t gcInfoIndex, const char* typeName)
{
    ASSERT(state->isAllocationAllowed());
    ASSERT(arenaIndex != BlinkGC::LargeObjectArenaIndex);
    NormalPageArena* arena = static_cast<NormalPageArena*>(state->arena(arenaIndex));
    Address address = arena->allocateObject(allocationSizeFromSize(size), gcInfoIndex);
    HeapAllocHooks::allocationHookIfEnabled(address, size, typeName);
    return address;
}

template<typename T>
Address ThreadHeap::allocate(size_t size, bool eagerlySweep)
{
    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    const char* typeName = WTF_HEAP_PROFILER_TYPE_NAME(T);
    return ThreadHeap::allocateOnArenaIndex(state, size, eagerlySweep ? BlinkGC::EagerSweepArenaIndex : ThreadHeap::arenaIndexForObjectSize(size), GCInfoTrait<T>::index(), typeName);
}

template<typename T>
Address ThreadHeap::reallocate(void* previous, size_t size)
{
    // Not intended to be a full C realloc() substitute;
    // realloc(nullptr, size) is not a supported alias for malloc(size).

    // TODO(sof): promptly free the previous object.
    if (!size) {
        // If the new size is 0 this is considered equivalent to free(previous).
        return nullptr;
    }

    ThreadState* state = ThreadStateFor<ThreadingTrait<T>::Affinity>::state();
    HeapObjectHeader* previousHeader = HeapObjectHeader::fromPayload(previous);
    BasePage* page = pageFromObject(previousHeader);
    ASSERT(page);

    // Determine arena index of new allocation.
    int arenaIndex;
    if (size >= largeObjectSizeThreshold) {
        arenaIndex = BlinkGC::LargeObjectArenaIndex;
    } else {
        arenaIndex = page->arena()->arenaIndex();
        if (isNormalArenaIndex(arenaIndex) || arenaIndex == BlinkGC::LargeObjectArenaIndex)
            arenaIndex = arenaIndexForObjectSize(size);
    }

    size_t gcInfoIndex = GCInfoTrait<T>::index();
    // TODO(haraken): We don't support reallocate() for finalizable objects.
    ASSERT(!ThreadHeap::gcInfo(previousHeader->gcInfoIndex())->hasFinalizer());
    ASSERT(previousHeader->gcInfoIndex() == gcInfoIndex);
    HeapAllocHooks::freeHookIfEnabled(static_cast<Address>(previous));
    Address address;
    if (arenaIndex == BlinkGC::LargeObjectArenaIndex) {
        address = page->arena()->allocateLargeObject(allocationSizeFromSize(size), gcInfoIndex);
    } else {
        const char* typeName = WTF_HEAP_PROFILER_TYPE_NAME(T);
        address = ThreadHeap::allocateOnArenaIndex(state, size, arenaIndex, gcInfoIndex, typeName);
    }
    size_t copySize = previousHeader->payloadSize();
    if (copySize > size)
        copySize = size;
    memcpy(address, previous, copySize);
    return address;
}

template<typename Derived>
template<typename T>
void VisitorHelper<Derived>::handleWeakCell(Visitor* self, void* object)
{
    T** cell = reinterpret_cast<T**>(object);
    if (*cell && !ObjectAliveTrait<T>::isHeapObjectAlive(*cell))
        *cell = nullptr;
}

} // namespace blink

#endif // Heap_h
