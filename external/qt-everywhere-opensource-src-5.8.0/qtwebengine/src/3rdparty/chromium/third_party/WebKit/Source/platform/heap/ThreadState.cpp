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

#include "platform/heap/ThreadState.h"

#include "base/trace_event/process_memory_dump.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/BlinkGCMemoryDumpProvider.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/Visitor.h"
#include "platform/web_memory_allocator_dump.h"
#include "platform/web_process_memory_dump.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/DataLog.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/allocator/Partitions.h"
#include <memory>
#include <v8.h>

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if OS(FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = nullptr;
uintptr_t ThreadState::s_mainThreadStackStart = 0;
uintptr_t ThreadState::s_mainThreadUnderestimatedStackSize = 0;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];

ThreadState::ThreadState(bool perThreadHeapEnabled)
    : m_thread(currentThread())
    , m_persistentRegion(wrapUnique(new PersistentRegion()))
#if OS(WIN) && COMPILER(MSVC)
    , m_threadStackSize(0)
#endif
    , m_startOfStack(reinterpret_cast<intptr_t*>(StackFrameDepth::getStackStart()))
    , m_endOfStack(reinterpret_cast<intptr_t*>(StackFrameDepth::getStackStart()))
    , m_safePointScopeMarker(nullptr)
    , m_atSafePoint(false)
    , m_interruptors()
    , m_sweepForbidden(false)
    , m_noAllocationCount(0)
    , m_gcForbiddenCount(0)
    , m_accumulatedSweepingTime(0)
    , m_vectorBackingArenaIndex(BlinkGC::Vector1ArenaIndex)
    , m_currentArenaAges(0)
    , m_perThreadHeapEnabled(perThreadHeapEnabled)
    , m_isTerminating(false)
    , m_gcMixinMarker(nullptr)
    , m_shouldFlushHeapDoesNotContainCache(false)
    , m_gcState(NoGCScheduled)
    , m_isolate(nullptr)
    , m_traceDOMWrappers(nullptr)
    , m_invalidateDeadObjectsInWrappersMarkingDeque(nullptr)
#if defined(ADDRESS_SANITIZER)
    , m_asanFakeStack(__asan_get_current_fake_stack())
#endif
#if defined(LEAK_SANITIZER)
    , m_disabledStaticPersistentsRegistration(0)
#endif
    , m_allocatedObjectSize(0)
    , m_markedObjectSize(0)
    , m_reportedMemoryToV8(0)
{
    ASSERT(checkThread());
    ASSERT(!**s_threadSpecific);
    **s_threadSpecific = this;

    if (m_perThreadHeapEnabled) {
        m_heap = new ThreadHeap();
    } else if (isMainThread()) {
        s_mainThreadStackStart = reinterpret_cast<uintptr_t>(m_startOfStack) - sizeof(void*);
        size_t underestimatedStackSize = StackFrameDepth::getUnderestimatedStackSize();
        if (underestimatedStackSize > sizeof(void*))
            s_mainThreadUnderestimatedStackSize = underestimatedStackSize - sizeof(void*);
        m_heap = new ThreadHeap();
    } else {
        m_heap = &ThreadState::mainThreadState()->heap();
    }
    ASSERT(m_heap);
    m_heap->attach(this);

    for (int arenaIndex = 0; arenaIndex < BlinkGC::LargeObjectArenaIndex; arenaIndex++)
        m_arenas[arenaIndex] = new NormalPageArena(this, arenaIndex);
    m_arenas[BlinkGC::LargeObjectArenaIndex] = new LargeObjectArena(this, BlinkGC::LargeObjectArenaIndex);

    m_likelyToBePromptlyFreed = wrapArrayUnique(new int[likelyToBePromptlyFreedArraySize]);
    clearArenaAges();

    // There is little use of weak references and collections off the main thread;
    // use a much lower initial block reservation.
    size_t initialBlockSize = isMainThread() ? CallbackStack::kDefaultBlockSize : CallbackStack::kMinimalBlockSize;
    m_threadLocalWeakCallbackStack = new CallbackStack(initialBlockSize);
}

ThreadState::~ThreadState()
{
    ASSERT(checkThread());
    delete m_threadLocalWeakCallbackStack;
    m_threadLocalWeakCallbackStack = nullptr;
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        delete m_arenas[i];

    **s_threadSpecific = nullptr;
    if (isMainThread()) {
        s_mainThreadStackStart = 0;
        s_mainThreadUnderestimatedStackSize = 0;
    }
}

#if OS(WIN) && COMPILER(MSVC)
size_t ThreadState::threadStackSize()
{
    if (m_threadStackSize)
        return m_threadStackSize;

    // Notice that we cannot use the TIB's StackLimit for the stack end, as it
    // tracks the end of the committed range. We're after the end of the reserved
    // stack area (most of which will be uncommitted, most times.)
    MEMORY_BASIC_INFORMATION stackInfo;
    memset(&stackInfo, 0, sizeof(MEMORY_BASIC_INFORMATION));
    size_t resultSize = VirtualQuery(&stackInfo, &stackInfo, sizeof(MEMORY_BASIC_INFORMATION));
    ASSERT_UNUSED(resultSize, resultSize >= sizeof(MEMORY_BASIC_INFORMATION));
    Address stackEnd = reinterpret_cast<Address>(stackInfo.AllocationBase);

    Address stackStart = reinterpret_cast<Address>(StackFrameDepth::getStackStart());
    RELEASE_ASSERT(stackStart && stackStart > stackEnd);
    m_threadStackSize = static_cast<size_t>(stackStart - stackEnd);
    // When the third last page of the reserved stack is accessed as a
    // guard page, the second last page will be committed (along with removing
    // the guard bit on the third last) _and_ a stack overflow exception
    // is raised.
    //
    // We have zero interest in running into stack overflow exceptions while
    // marking objects, so simply consider the last three pages + one above
    // as off-limits and adjust the reported stack size accordingly.
    //
    // http://blogs.msdn.com/b/satyem/archive/2012/08/13/thread-s-stack-memory-management.aspx
    // explains the details.
    RELEASE_ASSERT(m_threadStackSize > 4 * 0x1000);
    m_threadStackSize -= 4 * 0x1000;
    return m_threadStackSize;
}
#endif

void ThreadState::attachMainThread()
{
    RELEASE_ASSERT(!ProcessHeap::s_shutdownComplete);
    s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
    new (s_mainThreadStateStorage) ThreadState(false);
}

void ThreadState::attachCurrentThread(bool perThreadHeapEnabled)
{
    RELEASE_ASSERT(!ProcessHeap::s_shutdownComplete);
    new ThreadState(perThreadHeapEnabled);
}

void ThreadState::cleanupPages()
{
    ASSERT(checkThread());
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        m_arenas[i]->cleanupPages();
}

void ThreadState::runTerminationGC()
{
    if (isMainThread()) {
        cleanupPages();
        return;
    }
    ASSERT(checkThread());

    // Finish sweeping.
    completeSweep();

    releaseStaticPersistentNodes();

    // From here on ignore all conservatively discovered
    // pointers into the heap owned by this thread.
    m_isTerminating = true;

    // Set the terminate flag on all heap pages of this thread. This is used to
    // ensure we don't trace pages on other threads that are not part of the
    // thread local GC.
    prepareForThreadStateTermination();

    ProcessHeap::crossThreadPersistentRegion().prepareForThreadStateTermination(this);

    // Do thread local GC's as long as the count of thread local Persistents
    // changes and is above zero.
    int oldCount = -1;
    int currentCount = getPersistentRegion()->numberOfPersistents();
    ASSERT(currentCount >= 0);
    while (currentCount != oldCount) {
        ThreadHeap::collectGarbageForTerminatingThread(this);
        // Release the thread-local static persistents that were
        // instantiated while running the termination GC.
        releaseStaticPersistentNodes();
        oldCount = currentCount;
        currentCount = getPersistentRegion()->numberOfPersistents();
    }
    // We should not have any persistents left when getting to this point,
    // if we have it is probably a bug so adding a debug ASSERT to catch this.
    ASSERT(!currentCount);
    // All of pre-finalizers should be consumed.
    ASSERT(m_orderedPreFinalizers.isEmpty());
    RELEASE_ASSERT(gcState() == NoGCScheduled);

    // Add pages to the orphaned page pool to ensure any global GCs from this point
    // on will not trace objects on this thread's arenas.
    cleanupPages();
}

void ThreadState::cleanupMainThread()
{
    ASSERT(isMainThread());

#if defined(LEAK_SANITIZER)
    // See comment below, clear out most garbage before releasing static
    // persistents should some of the finalizers depend on touching
    // these persistents.
    ThreadHeap::collectAllGarbage();
#endif

    releaseStaticPersistentNodes();

#if defined(LEAK_SANITIZER)
    // If LSan is about to perform leak detection, after having released all
    // the registered static Persistent<> root references to global caches
    // that Blink keeps, follow up with a round of GCs to clear out all
    // what they referred to.
    //
    // This is not needed for caches over non-Oilpan objects, as they're
    // not scanned by LSan due to being held in non-global storage
    // ("static" references inside functions/methods.)
    ThreadHeap::collectAllGarbage();
#endif

    // Finish sweeping before shutting down V8. Otherwise, some destructor
    // may access V8 and cause crashes.
    completeSweep();

    // It is unsafe to trigger GCs after this point because some
    // destructor may access already-detached V8 and cause crashes.
    // Also it is useless. So we forbid GCs.
    enterGCForbiddenScope();
}

void ThreadState::detachMainThread()
{
    // Enter a safe point before trying to acquire threadAttachMutex
    // to avoid dead lock if another thread is preparing for GC, has acquired
    // threadAttachMutex and waiting for other threads to pause or reach a
    // safepoint.
    ThreadState* state = mainThreadState();
    ASSERT(!state->isSweepingInProgress());

    state->heap().detach(state);
    state->~ThreadState();
}

void ThreadState::detachCurrentThread()
{
    ThreadState* state = current();
    state->heap().detach(state);
    RELEASE_ASSERT(state->gcState() == ThreadState::NoGCScheduled);
    delete state;
}

NO_SANITIZE_ADDRESS
void ThreadState::visitAsanFakeStackForPointer(Visitor* visitor, Address ptr)
{
#if defined(ADDRESS_SANITIZER)
    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* fakeFrameStart = nullptr;
    Address* fakeFrameEnd = nullptr;
    Address* maybeFakeFrame = reinterpret_cast<Address*>(ptr);
    Address* realFrameForFakeFrame =
        reinterpret_cast<Address*>(
            __asan_addr_is_in_fake_stack(
                m_asanFakeStack, maybeFakeFrame,
                reinterpret_cast<void**>(&fakeFrameStart),
                reinterpret_cast<void**>(&fakeFrameEnd)));
    if (realFrameForFakeFrame) {
        // This is a fake frame from the asan fake stack.
        if (realFrameForFakeFrame > end && start > realFrameForFakeFrame) {
            // The real stack address for the asan fake frame is
            // within the stack range that we need to scan so we need
            // to visit the values in the fake frame.
            for (Address* p = fakeFrameStart; p < fakeFrameEnd; ++p)
                m_heap->checkAndMarkPointer(visitor, *p);
        }
    }
#endif
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_THREAD
void ThreadState::visitStack(Visitor* visitor)
{
    if (m_stackState == BlinkGC::NoHeapPointersOnStack)
        return;

    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    // If there is a safepoint scope marker we should stop the stack
    // scanning there to not touch active parts of the stack. Anything
    // interesting beyond that point is in the safepoint stack copy.
    // If there is no scope marker the thread is blocked and we should
    // scan all the way to the recorded end stack pointer.
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* safePointScopeMarker = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* current = safePointScopeMarker ? safePointScopeMarker : end;

    // Ensure that current is aligned by address size otherwise the loop below
    // will read past start address.
    current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) & ~(sizeof(Address) - 1));

    for (; current < start; ++current) {
        Address ptr = *current;
#if defined(MEMORY_SANITIZER)
        // |ptr| may be uninitialized by design. Mark it as initialized to keep
        // MSan from complaining.
        // Note: it may be tempting to get rid of |ptr| and simply use |current|
        // here, but that would be incorrect. We intentionally use a local
        // variable because we don't want to unpoison the original stack.
        __msan_unpoison(&ptr, sizeof(ptr));
#endif
        m_heap->checkAndMarkPointer(visitor, ptr);
        visitAsanFakeStackForPointer(visitor, ptr);
    }

    for (Address ptr : m_safePointStackCopy) {
#if defined(MEMORY_SANITIZER)
        // See the comment above.
        __msan_unpoison(&ptr, sizeof(ptr));
#endif
        m_heap->checkAndMarkPointer(visitor, ptr);
        visitAsanFakeStackForPointer(visitor, ptr);
    }
}

void ThreadState::visitPersistents(Visitor* visitor)
{
    m_persistentRegion->tracePersistentNodes(visitor);
    if (m_traceDOMWrappers) {
        TRACE_EVENT0("blink_gc", "V8GCController::traceDOMWrappers");
        m_traceDOMWrappers(m_isolate, visitor);
    }
}

ThreadState::GCSnapshotInfo::GCSnapshotInfo(size_t numObjectTypes)
    : liveCount(Vector<int>(numObjectTypes))
    , deadCount(Vector<int>(numObjectTypes))
    , liveSize(Vector<size_t>(numObjectTypes))
    , deadSize(Vector<size_t>(numObjectTypes))
{
}

void ThreadState::pushThreadLocalWeakCallback(void* object, WeakCallback callback)
{
    CallbackStack::Item* slot = m_threadLocalWeakCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool ThreadState::popAndInvokeThreadLocalWeakCallback(Visitor* visitor)
{
    ASSERT(checkThread());
    // For weak processing we should never reach orphaned pages since orphaned
    // pages are not traced and thus objects on those pages are never be
    // registered as objects on orphaned pages. We cannot assert this here since
    // we might have an off-heap collection. We assert it in
    // ThreadHeap::pushThreadLocalWeakCallback.
    if (CallbackStack::Item* item = m_threadLocalWeakCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

void ThreadState::threadLocalWeakProcessing()
{
    ASSERT(checkThread());
    ASSERT(!sweepForbidden());
    TRACE_EVENT0("blink_gc", "ThreadState::threadLocalWeakProcessing");
    double startTime = WTF::currentTimeMS();

    SweepForbiddenScope sweepForbiddenScope(this);
    ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

    // Disallow allocation during weak processing.
    // It would be technically safe to allow allocations, but it is unsafe
    // to mutate an object graph in a way in which a dead object gets
    // resurrected or mutate a HashTable (because HashTable's weak processing
    // assumes that the HashTable hasn't been mutated since the latest marking).
    // Due to the complexity, we just forbid allocations.
    NoAllocationScope noAllocationScope(this);

    std::unique_ptr<Visitor> visitor = Visitor::create(this, BlinkGC::ThreadLocalWeakProcessing);

    // Perform thread-specific weak processing.
    while (popAndInvokeThreadLocalWeakCallback(visitor.get())) { }

    m_threadLocalWeakCallbackStack->decommit();

    if (isMainThread()) {
        double timeForThreadLocalWeakProcessing = WTF::currentTimeMS() - startTime;
        DEFINE_STATIC_LOCAL(CustomCountHistogram, timeForWeakHistogram, ("BlinkGC.TimeForThreadLocalWeakProcessing", 1, 10 * 1000, 50));
        timeForWeakHistogram.count(timeForThreadLocalWeakProcessing);
    }
}

size_t ThreadState::totalMemorySize()
{
    return m_heap->heapStats().allocatedObjectSize() + m_heap->heapStats().markedObjectSize() + WTF::Partitions::totalSizeOfCommittedPages();
}

size_t ThreadState::estimatedLiveSize(size_t estimationBaseSize, size_t sizeAtLastGC)
{
    if (m_heap->heapStats().wrapperCountAtLastGC() == 0) {
        // We'll reach here only before hitting the first GC.
        return 0;
    }

    // (estimated size) = (estimation base size) - (heap size at the last GC) / (# of persistent handles at the last GC) * (# of persistent handles collected since the last GC);
    size_t sizeRetainedByCollectedPersistents = static_cast<size_t>(1.0 * sizeAtLastGC / m_heap->heapStats().wrapperCountAtLastGC() * m_heap->heapStats().collectedWrapperCount());
    if (estimationBaseSize < sizeRetainedByCollectedPersistents)
        return 0;
    return estimationBaseSize - sizeRetainedByCollectedPersistents;
}

double ThreadState::heapGrowingRate()
{
    size_t currentSize = m_heap->heapStats().allocatedObjectSize() + m_heap->heapStats().markedObjectSize();
    size_t estimatedSize = estimatedLiveSize(m_heap->heapStats().markedObjectSizeAtLastCompleteSweep(), m_heap->heapStats().markedObjectSizeAtLastCompleteSweep());

    // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
    double growingRate = estimatedSize > 0 ? 1.0 * currentSize / estimatedSize : 100;
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::heapEstimatedSizeKB", std::min(estimatedSize / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::heapGrowingRate", static_cast<int>(100 * growingRate));
    return growingRate;
}

double ThreadState::partitionAllocGrowingRate()
{
    size_t currentSize = WTF::Partitions::totalSizeOfCommittedPages();
    size_t estimatedSize = estimatedLiveSize(currentSize, m_heap->heapStats().partitionAllocSizeAtLastGC());

    // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
    double growingRate = estimatedSize > 0 ? 1.0 * currentSize / estimatedSize : 100;
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::partitionAllocEstimatedSizeKB", std::min(estimatedSize / 1024, static_cast<size_t>(INT_MAX)));
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::partitionAllocGrowingRate", static_cast<int>(100 * growingRate));
    return growingRate;
}

// TODO(haraken): We should improve the GC heuristics. The heuristics affect
// performance significantly.
bool ThreadState::judgeGCThreshold(size_t totalMemorySizeThreshold, double heapGrowingRateThreshold)
{
    // If the allocated object size or the total memory size is small, don't trigger a GC.
    if (m_heap->heapStats().allocatedObjectSize() < 100 * 1024 || totalMemorySize() < totalMemorySizeThreshold)
        return false;
    // If the growing rate of Oilpan's heap or PartitionAlloc is high enough,
    // trigger a GC.
#if PRINT_HEAP_STATS
    dataLogF("heapGrowingRate=%.1lf, partitionAllocGrowingRate=%.1lf\n", heapGrowingRate(), partitionAllocGrowingRate());
#endif
    return heapGrowingRate() >= heapGrowingRateThreshold || partitionAllocGrowingRate() >= heapGrowingRateThreshold;
}

bool ThreadState::shouldScheduleIdleGC()
{
    if (gcState() != NoGCScheduled)
        return false;
    return judgeGCThreshold(1024 * 1024, 1.5);
}

bool ThreadState::shouldScheduleV8FollowupGC()
{
    return judgeGCThreshold(32 * 1024 * 1024, 1.5);
}

bool ThreadState::shouldSchedulePageNavigationGC(float estimatedRemovalRatio)
{
    // If estimatedRemovalRatio is low we should let IdleGC handle this.
    if (estimatedRemovalRatio < 0.01)
        return false;
    return judgeGCThreshold(32 * 1024 * 1024, 1.5 * (1 - estimatedRemovalRatio));
}

bool ThreadState::shouldForceConservativeGC()
{
    // TODO(haraken): 400% is too large. Lower the heap growing factor.
    return judgeGCThreshold(32 * 1024 * 1024, 5.0);
}

// If we're consuming too much memory, trigger a conservative GC
// aggressively. This is a safe guard to avoid OOM.
bool ThreadState::shouldForceMemoryPressureGC()
{
    if (totalMemorySize() < 300 * 1024 * 1024)
        return false;
    return judgeGCThreshold(0, 1.5);
}

void ThreadState::scheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType gcType)
{
    ASSERT(checkThread());
    ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
    dataLogF("ThreadState::scheduleV8FollowupGCIfNeeded (gcType=%s)\n", gcType == BlinkGC::V8MajorGC ? "MajorGC" : "MinorGC");
#endif

    if (isGCForbidden())
        return;

    // This completeSweep() will do nothing in common cases since we've
    // called completeSweep() before V8 starts minor/major GCs.
    completeSweep();
    ASSERT(!isSweepingInProgress());
    ASSERT(!sweepForbidden());

    if ((gcType == BlinkGC::V8MajorGC && shouldForceMemoryPressureGC())
        || shouldScheduleV8FollowupGC()) {
#if PRINT_HEAP_STATS
        dataLogF("Scheduled PreciseGC\n");
#endif
        schedulePreciseGC();
        return;
    }
    if (gcType == BlinkGC::V8MajorGC && shouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
        dataLogF("Scheduled IdleGC\n");
#endif
        scheduleIdleGC();
        return;
    }
}

void ThreadState::willStartV8GC(BlinkGC::V8GCType gcType)
{
    // Finish Oilpan's complete sweeping before running a V8 major GC.
    // This will let the GC collect more V8 objects.
    //
    // TODO(haraken): It's a bit too late for a major GC to schedule
    // completeSweep() here, because gcPrologue for a major GC is called
    // not at the point where the major GC started but at the point where
    // the major GC requests object grouping.
    if (gcType == BlinkGC::V8MajorGC)
        completeSweep();
}

void ThreadState::schedulePageNavigationGCIfNeeded(float estimatedRemovalRatio)
{
    ASSERT(checkThread());
    ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
    dataLogF("ThreadState::schedulePageNavigationGCIfNeeded (estimatedRemovalRatio=%.2lf)\n", estimatedRemovalRatio);
#endif

    if (isGCForbidden())
        return;

    // Finish on-going lazy sweeping.
    // TODO(haraken): It might not make sense to force completeSweep() for all
    // page navigations.
    completeSweep();
    ASSERT(!isSweepingInProgress());
    ASSERT(!sweepForbidden());

    if (shouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
        dataLogF("Scheduled MemoryPressureGC\n");
#endif
        ThreadHeap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep, BlinkGC::MemoryPressureGC);
        return;
    }
    if (shouldSchedulePageNavigationGC(estimatedRemovalRatio)) {
#if PRINT_HEAP_STATS
        dataLogF("Scheduled PageNavigationGC\n");
#endif
        schedulePageNavigationGC();
    }
}

void ThreadState::schedulePageNavigationGC()
{
    ASSERT(checkThread());
    ASSERT(!isSweepingInProgress());
    setGCState(PageNavigationGCScheduled);
}

void ThreadState::scheduleGCIfNeeded()
{
    ASSERT(checkThread());
    ThreadHeap::reportMemoryUsageForTracing();

#if PRINT_HEAP_STATS
    dataLogF("ThreadState::scheduleGCIfNeeded\n");
#endif

    // Allocation is allowed during sweeping, but those allocations should not
    // trigger nested GCs.
    if (isGCForbidden())
        return;

    if (isSweepingInProgress())
        return;
    ASSERT(!sweepForbidden());

    reportMemoryToV8();

    if (shouldForceMemoryPressureGC()) {
        completeSweep();
        if (shouldForceMemoryPressureGC()) {
#if PRINT_HEAP_STATS
            dataLogF("Scheduled MemoryPressureGC\n");
#endif
            ThreadHeap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep, BlinkGC::MemoryPressureGC);
            return;
        }
    }

    if (shouldForceConservativeGC()) {
        completeSweep();
        if (shouldForceConservativeGC()) {
#if PRINT_HEAP_STATS
            dataLogF("Scheduled ConservativeGC\n");
#endif
            ThreadHeap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithoutSweep, BlinkGC::ConservativeGC);
            return;
        }
    }
    if (shouldScheduleIdleGC()) {
#if PRINT_HEAP_STATS
        dataLogF("Scheduled IdleGC\n");
#endif
        scheduleIdleGC();
        return;
    }
}

ThreadState* ThreadState::fromObject(const void* object)
{
    ASSERT(object);
    BasePage* page = pageFromObject(object);
    ASSERT(page);
    ASSERT(page->arena());
    return page->arena()->getThreadState();
}

void ThreadState::performIdleGC(double deadlineSeconds)
{
    ASSERT(checkThread());
    ASSERT(isMainThread());
    ASSERT(Platform::current()->currentThread()->scheduler());

    if (gcState() != IdleGCScheduled)
        return;

    if (isGCForbidden()) {
        // If GC is forbidden at this point, try again.
        scheduleIdleGC();
        return;
    }

    double idleDeltaInSeconds = deadlineSeconds - monotonicallyIncreasingTime();
    if (idleDeltaInSeconds <= m_heap->heapStats().estimatedMarkingTime() && !Platform::current()->currentThread()->scheduler()->canExceedIdleDeadlineIfRequired()) {
        // If marking is estimated to take longer than the deadline and we can't
        // exceed the deadline, then reschedule for the next idle period.
        scheduleIdleGC();
        return;
    }

    TRACE_EVENT2("blink_gc", "ThreadState::performIdleGC", "idleDeltaInSeconds", idleDeltaInSeconds, "estimatedMarkingTime", m_heap->heapStats().estimatedMarkingTime());
    ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithoutSweep, BlinkGC::IdleGC);
}

void ThreadState::performIdleLazySweep(double deadlineSeconds)
{
    ASSERT(checkThread());
    ASSERT(isMainThread());

    // If we are not in a sweeping phase, there is nothing to do here.
    if (!isSweepingInProgress())
        return;

    // This check is here to prevent performIdleLazySweep() from being called
    // recursively. I'm not sure if it can happen but it would be safer to have
    // the check just in case.
    if (sweepForbidden())
        return;

    TRACE_EVENT1("blink_gc,devtools.timeline", "ThreadState::performIdleLazySweep", "idleDeltaInSeconds", deadlineSeconds - monotonicallyIncreasingTime());

    bool sweepCompleted = true;
    SweepForbiddenScope scope(this);
    {
        double startTime = WTF::currentTimeMS();
        ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

        for (int i = 0; i < BlinkGC::NumberOfArenas; i++) {
            // lazySweepWithDeadline() won't check the deadline until it sweeps
            // 10 pages. So we give a small slack for safety.
            double slack = 0.001;
            double remainingBudget = deadlineSeconds - slack - monotonicallyIncreasingTime();
            if (remainingBudget <= 0 || !m_arenas[i]->lazySweepWithDeadline(deadlineSeconds)) {
                // We couldn't finish the sweeping within the deadline.
                // We request another idle task for the remaining sweeping.
                scheduleIdleLazySweep();
                sweepCompleted = false;
                break;
            }
        }

        accumulateSweepingTime(WTF::currentTimeMS() - startTime);
    }

    if (sweepCompleted)
        postSweep();
}

void ThreadState::scheduleIdleGC()
{
    // TODO(haraken): Idle GC should be supported in worker threads as well.
    if (!isMainThread())
        return;

    if (isSweepingInProgress()) {
        setGCState(SweepingAndIdleGCScheduled);
        return;
    }

    // Some threads (e.g. PPAPI thread) don't have a scheduler.
    if (!Platform::current()->currentThread()->scheduler())
        return;

    Platform::current()->currentThread()->scheduler()->postNonNestableIdleTask(BLINK_FROM_HERE, WTF::bind(&ThreadState::performIdleGC, WTF::unretained(this)));
    setGCState(IdleGCScheduled);
}

void ThreadState::scheduleIdleLazySweep()
{
    // TODO(haraken): Idle complete sweep should be supported in worker threads.
    if (!isMainThread())
        return;

    // Some threads (e.g. PPAPI thread) don't have a scheduler.
    if (!Platform::current()->currentThread()->scheduler())
        return;

    Platform::current()->currentThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind(&ThreadState::performIdleLazySweep, WTF::unretained(this)));
}

void ThreadState::schedulePreciseGC()
{
    ASSERT(checkThread());
    if (isSweepingInProgress()) {
        setGCState(SweepingAndPreciseGCScheduled);
        return;
    }

    setGCState(PreciseGCScheduled);
}

namespace {

#define UNEXPECTED_GCSTATE(s) case ThreadState::s: RELEASE_NOTREACHED() << "Unexpected transition while in GCState " #s; return

void unexpectedGCState(ThreadState::GCState gcState)
{
    switch (gcState) {
        UNEXPECTED_GCSTATE(NoGCScheduled);
        UNEXPECTED_GCSTATE(IdleGCScheduled);
        UNEXPECTED_GCSTATE(PreciseGCScheduled);
        UNEXPECTED_GCSTATE(FullGCScheduled);
        UNEXPECTED_GCSTATE(GCRunning);
        UNEXPECTED_GCSTATE(EagerSweepScheduled);
        UNEXPECTED_GCSTATE(LazySweepScheduled);
        UNEXPECTED_GCSTATE(Sweeping);
        UNEXPECTED_GCSTATE(SweepingAndIdleGCScheduled);
        UNEXPECTED_GCSTATE(SweepingAndPreciseGCScheduled);
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

#undef UNEXPECTED_GCSTATE

} // namespace

#define VERIFY_STATE_TRANSITION(condition) if (UNLIKELY(!(condition))) unexpectedGCState(m_gcState)

void ThreadState::setGCState(GCState gcState)
{
    switch (gcState) {
    case NoGCScheduled:
        ASSERT(checkThread());
        VERIFY_STATE_TRANSITION(m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled);
        break;
    case IdleGCScheduled:
    case PreciseGCScheduled:
    case FullGCScheduled:
    case PageNavigationGCScheduled:
        ASSERT(checkThread());
        VERIFY_STATE_TRANSITION(m_gcState == NoGCScheduled || m_gcState == IdleGCScheduled || m_gcState == PreciseGCScheduled || m_gcState == FullGCScheduled || m_gcState == PageNavigationGCScheduled || m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled || m_gcState == SweepingAndPreciseGCScheduled);
        completeSweep();
        break;
    case GCRunning:
        ASSERT(!isInGC());
        VERIFY_STATE_TRANSITION(m_gcState != GCRunning);
        break;
    case EagerSweepScheduled:
    case LazySweepScheduled:
        ASSERT(isInGC());
        VERIFY_STATE_TRANSITION(m_gcState == GCRunning);
        break;
    case Sweeping:
        ASSERT(checkThread());
        VERIFY_STATE_TRANSITION(m_gcState == EagerSweepScheduled || m_gcState == LazySweepScheduled);
        break;
    case SweepingAndIdleGCScheduled:
    case SweepingAndPreciseGCScheduled:
        ASSERT(checkThread());
        VERIFY_STATE_TRANSITION(m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled || m_gcState == SweepingAndPreciseGCScheduled);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    m_gcState = gcState;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::runScheduledGC(BlinkGC::StackState stackState)
{
    ASSERT(checkThread());
    if (stackState != BlinkGC::NoHeapPointersOnStack)
        return;

    // If a safe point is entered while initiating a GC, we clearly do
    // not want to do another as part that -- the safe point is only
    // entered after checking if a scheduled GC ought to run first.
    // Prevent that from happening by marking GCs as forbidden while
    // one is initiated and later running.
    if (isGCForbidden())
        return;

    switch (gcState()) {
    case FullGCScheduled:
        ThreadHeap::collectAllGarbage();
        break;
    case PreciseGCScheduled:
        ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithoutSweep, BlinkGC::PreciseGC);
        break;
    case PageNavigationGCScheduled:
        ThreadHeap::collectGarbage(BlinkGC::NoHeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::PageNavigationGC);
        break;
    case IdleGCScheduled:
        // Idle time GC will be scheduled by Blink Scheduler.
        break;
    default:
        break;
    }
}

void ThreadState::flushHeapDoesNotContainCacheIfNeeded()
{
    if (m_shouldFlushHeapDoesNotContainCache) {
        m_heap->flushHeapDoesNotContainCache();
        m_shouldFlushHeapDoesNotContainCache = false;
    }
}

void ThreadState::makeConsistentForGC()
{
    ASSERT(isInGC());
    TRACE_EVENT0("blink_gc", "ThreadState::makeConsistentForGC");
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        m_arenas[i]->makeConsistentForGC();
}

void ThreadState::makeConsistentForMutator()
{
    ASSERT(isInGC());
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        m_arenas[i]->makeConsistentForMutator();
}

void ThreadState::preGC()
{
    ASSERT(!isInGC());
    setGCState(GCRunning);
    makeConsistentForGC();
    flushHeapDoesNotContainCacheIfNeeded();
    clearArenaAges();

    // It is possible, albeit rare, for a thread to be kept
    // at a safepoint across multiple GCs, as resuming all attached
    // threads after the "global" GC phases will contend for the shared
    // safepoint barrier mutexes etc, which can additionally delay
    // a thread. Enough so that another thread may initiate
    // a new GC before this has happened.
    //
    // In which case the parked thread's ThreadState will have unprocessed
    // entries on its local weak callback stack when that later GC goes
    // ahead. Clear out and invalidate the stack now, as the thread
    // should only process callbacks that's found to be reachable by
    // the latest GC, when it eventually gets to next perform
    // thread-local weak processing.
    m_threadLocalWeakCallbackStack->clear();
}

void ThreadState::postGC(BlinkGC::GCType gcType)
{
    if (RuntimeEnabledFeatures::traceWrappablesEnabled()
        && m_invalidateDeadObjectsInWrappersMarkingDeque) {
        m_invalidateDeadObjectsInWrappersMarkingDeque(m_isolate);
    }

    ASSERT(isInGC());
    for (int i = 0; i < BlinkGC::NumberOfArenas; i++)
        m_arenas[i]->prepareForSweep();

    if (gcType == BlinkGC::GCWithSweep) {
        setGCState(EagerSweepScheduled);
    } else if (gcType == BlinkGC::GCWithoutSweep) {
        setGCState(LazySweepScheduled);
    } else {
        takeSnapshot(SnapshotType::HeapSnapshot);

        // This unmarks all marked objects and marks all unmarked objects dead.
        makeConsistentForMutator();

        takeSnapshot(SnapshotType::FreelistSnapshot);

        // Force setting NoGCScheduled to circumvent checkThread()
        // in setGCState().
        m_gcState = NoGCScheduled;
    }
}

void ThreadState::preSweep()
{
    ASSERT(checkThread());
    if (gcState() != EagerSweepScheduled && gcState() != LazySweepScheduled)
        return;

    threadLocalWeakProcessing();

    GCState previousGCState = gcState();
    // We have to set the GCState to Sweeping before calling pre-finalizers
    // to disallow a GC during the pre-finalizers.
    setGCState(Sweeping);

    // Allocation is allowed during the pre-finalizers and destructors.
    // However, they must not mutate an object graph in a way in which
    // a dead object gets resurrected.
    invokePreFinalizers();

    m_accumulatedSweepingTime = 0;

    eagerSweep();

#if defined(ADDRESS_SANITIZER)
    poisonAllHeaps();
#endif
    if (previousGCState == EagerSweepScheduled) {
        // Eager sweeping should happen only in testing.
        completeSweep();
    } else {
        // The default behavior is lazy sweeping.
        scheduleIdleLazySweep();
    }
}

#if defined(ADDRESS_SANITIZER)
void ThreadState::poisonAllHeaps()
{
    // Poisoning all unmarked objects in the other arenas.
    for (int i = 1; i < BlinkGC::NumberOfArenas; i++)
        m_arenas[i]->poisonArena();
}

void ThreadState::poisonEagerArena()
{
    m_arenas[BlinkGC::EagerSweepArenaIndex]->poisonArena();
}
#endif

void ThreadState::eagerSweep()
{
#if defined(ADDRESS_SANITIZER)
    poisonEagerArena();
#endif
    ASSERT(checkThread());
    // Some objects need to be finalized promptly and cannot be handled
    // by lazy sweeping. Keep those in a designated heap and sweep it
    // eagerly.
    ASSERT(isSweepingInProgress());

    // Mirroring the completeSweep() condition; see its comment.
    if (sweepForbidden())
        return;

    SweepForbiddenScope scope(this);
    ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

    double startTime = WTF::currentTimeMS();
    m_arenas[BlinkGC::EagerSweepArenaIndex]->completeSweep();
    accumulateSweepingTime(WTF::currentTimeMS() - startTime);
}

void ThreadState::completeSweep()
{
    ASSERT(checkThread());
    // If we are not in a sweeping phase, there is nothing to do here.
    if (!isSweepingInProgress())
        return;

    // completeSweep() can be called recursively if finalizers can allocate
    // memory and the allocation triggers completeSweep(). This check prevents
    // the sweeping from being executed recursively.
    if (sweepForbidden())
        return;

    SweepForbiddenScope scope(this);
    {
        ScriptForbiddenIfMainThreadScope scriptForbiddenScope;

        TRACE_EVENT0("blink_gc,devtools.timeline", "ThreadState::completeSweep");
        double startTime = WTF::currentTimeMS();

        static_assert(BlinkGC::EagerSweepArenaIndex == 0, "Eagerly swept arenas must be processed first.");
        for (int i = 0; i < BlinkGC::NumberOfArenas; i++)
            m_arenas[i]->completeSweep();

        double timeForCompleteSweep = WTF::currentTimeMS() - startTime;
        accumulateSweepingTime(timeForCompleteSweep);

        if (isMainThread()) {
            DEFINE_STATIC_LOCAL(CustomCountHistogram, completeSweepHistogram, ("BlinkGC.CompleteSweep", 1, 10 * 1000, 50));
            completeSweepHistogram.count(timeForCompleteSweep);
        }
    }

    postSweep();
}

void ThreadState::postSweep()
{
    ASSERT(checkThread());
    ThreadHeap::reportMemoryUsageForTracing();

    if (isMainThread()) {
        double collectionRate = 0;
        if (m_heap->heapStats().objectSizeAtLastGC() > 0)
            collectionRate = 1 - 1.0 * m_heap->heapStats().markedObjectSize() / m_heap->heapStats().objectSizeAtLastGC();
        TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "ThreadState::collectionRate", static_cast<int>(100 * collectionRate));

#if PRINT_HEAP_STATS
        dataLogF("ThreadState::postSweep (collectionRate=%d%%)\n", static_cast<int>(100 * collectionRate));
#endif

        // ThreadHeap::markedObjectSize() may be underestimated here if any other
        // thread has not yet finished lazy sweeping.
        m_heap->heapStats().setMarkedObjectSizeAtLastCompleteSweep(m_heap->heapStats().markedObjectSize());

        DEFINE_STATIC_LOCAL(CustomCountHistogram, objectSizeBeforeGCHistogram, ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
        objectSizeBeforeGCHistogram.count(m_heap->heapStats().objectSizeAtLastGC() / 1024);
        DEFINE_STATIC_LOCAL(CustomCountHistogram, objectSizeAfterGCHistogram, ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
        objectSizeAfterGCHistogram.count(m_heap->heapStats().markedObjectSize() / 1024);
        DEFINE_STATIC_LOCAL(CustomCountHistogram, collectionRateHistogram, ("BlinkGC.CollectionRate", 1, 100, 20));
        collectionRateHistogram.count(static_cast<int>(100 * collectionRate));
        DEFINE_STATIC_LOCAL(CustomCountHistogram, timeForSweepHistogram, ("BlinkGC.TimeForSweepingAllObjects", 1, 10 * 1000, 50));
        timeForSweepHistogram.count(m_accumulatedSweepingTime);


#define COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(GCReason)                \
    case BlinkGC::GCReason: {                                                 \
        DEFINE_STATIC_LOCAL(CustomCountHistogram, histogram,                  \
            ("BlinkGC.CollectionRate_" #GCReason, 1, 100, 20));             \
        histogram.count(static_cast<int>(100 * collectionRate));              \
        break;                                                                \
    }

        switch (m_heap->lastGCReason()) {
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(IdleGC)
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PreciseGC)
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ConservativeGC)
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(ForcedGC)
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(MemoryPressureGC)
            COUNT_COLLECTION_RATE_HISTOGRAM_BY_GC_REASON(PageNavigationGC)
        default:
            break;
        }
    }

    switch (gcState()) {
    case Sweeping:
        setGCState(NoGCScheduled);
        break;
    case SweepingAndPreciseGCScheduled:
        setGCState(PreciseGCScheduled);
        break;
    case SweepingAndIdleGCScheduled:
        setGCState(NoGCScheduled);
        scheduleIdleGC();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void ThreadState::prepareForThreadStateTermination()
{
    ASSERT(checkThread());
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        m_arenas[i]->prepareHeapForTermination();
}

#if ENABLE(ASSERT)
BasePage* ThreadState::findPageFromAddress(Address address)
{
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i) {
        if (BasePage* page = m_arenas[i]->findPageFromAddress(address))
            return page;
    }
    return nullptr;
}
#endif

size_t ThreadState::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    for (int i = 0; i < BlinkGC::NumberOfArenas; ++i)
        objectPayloadSize += m_arenas[i]->objectPayloadSizeForTesting();
    return objectPayloadSize;
}

void ThreadState::safePoint(BlinkGC::StackState stackState)
{
    ASSERT(checkThread());
    ThreadHeap::reportMemoryUsageForTracing();

    runScheduledGC(stackState);
    ASSERT(!m_atSafePoint);
    m_stackState = stackState;
    m_atSafePoint = true;
    m_heap->checkAndPark(this, nullptr);
    m_atSafePoint = false;
    m_stackState = BlinkGC::HeapPointersOnStack;
    preSweep();
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with detect_stack_use_after_return=1
// then stack marker obtained from SafePointScope will point into a fake stack.
// Detect this case by checking if it falls in between current stack frame
// and stack start and use an arbitrary high enough value for it.
// Don't adjust stack marker in any other case to match behavior of code running
// without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* adjustScopeMarkerForAdressSanitizer(void* scopeMarker)
{
    Address start = reinterpret_cast<Address>(StackFrameDepth::getStackStart());
    Address end = reinterpret_cast<Address>(&start);
    RELEASE_ASSERT(end < start);

    if (end <= scopeMarker && scopeMarker < start)
        return scopeMarker;

    // 256 is as good an approximation as any else.
    const size_t bytesToCopy = sizeof(Address) * 256;
    if (static_cast<size_t>(start - end) < bytesToCopy)
        return start;

    return end + bytesToCopy;
}
#endif

void ThreadState::enterSafePoint(BlinkGC::StackState stackState, void* scopeMarker)
{
    ASSERT(checkThread());
#ifdef ADDRESS_SANITIZER
    if (stackState == BlinkGC::HeapPointersOnStack)
        scopeMarker = adjustScopeMarkerForAdressSanitizer(scopeMarker);
#endif
    ASSERT(stackState == BlinkGC::NoHeapPointersOnStack || scopeMarker);
    runScheduledGC(stackState);
    ASSERT(!m_atSafePoint);
    m_atSafePoint = true;
    m_stackState = stackState;
    m_safePointScopeMarker = scopeMarker;
    m_heap->enterSafePoint(this);
}

void ThreadState::leaveSafePoint(SafePointAwareMutexLocker* locker)
{
    ASSERT(checkThread());
    ASSERT(m_atSafePoint);
    m_heap->leaveSafePoint(this, locker);
    m_atSafePoint = false;
    m_stackState = BlinkGC::HeapPointersOnStack;
    clearSafePointScopeMarker();
    preSweep();
}

void ThreadState::reportMemoryToV8()
{
    if (!m_isolate)
        return;

    size_t currentHeapSize = m_allocatedObjectSize + m_markedObjectSize;
    int64_t diff = static_cast<int64_t>(currentHeapSize) - static_cast<int64_t>(m_reportedMemoryToV8);
    m_isolate->AdjustAmountOfExternalAllocatedMemory(diff);
    m_reportedMemoryToV8 = currentHeapSize;
}

void ThreadState::resetHeapCounters()
{
    m_allocatedObjectSize = 0;
    m_markedObjectSize = 0;
}

void ThreadState::increaseAllocatedObjectSize(size_t delta)
{
    m_allocatedObjectSize += delta;
    m_heap->heapStats().increaseAllocatedObjectSize(delta);
}

void ThreadState::decreaseAllocatedObjectSize(size_t delta)
{
    m_allocatedObjectSize -= delta;
    m_heap->heapStats().decreaseAllocatedObjectSize(delta);
}

void ThreadState::increaseMarkedObjectSize(size_t delta)
{
    m_markedObjectSize += delta;
    m_heap->heapStats().increaseMarkedObjectSize(delta);
}

void ThreadState::copyStackUntilSafePointScope()
{
    if (!m_safePointScopeMarker || m_stackState == BlinkGC::NoHeapPointersOnStack)
        return;

    Address* to = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* from = reinterpret_cast<Address*>(m_endOfStack);
    RELEASE_ASSERT(from < to);
    RELEASE_ASSERT(to <= reinterpret_cast<Address*>(m_startOfStack));
    size_t slotCount = static_cast<size_t>(to - from);
    // Catch potential performance issues.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // ASan/LSan use more space on the stack and we therefore
    // increase the allowed stack copying for those builds.
    ASSERT(slotCount < 2048);
#else
    ASSERT(slotCount < 1024);
#endif

    ASSERT(!m_safePointStackCopy.size());
    m_safePointStackCopy.resize(slotCount);
    for (size_t i = 0; i < slotCount; ++i) {
        m_safePointStackCopy[i] = from[i];
    }
}

void ThreadState::addInterruptor(std::unique_ptr<BlinkGCInterruptor> interruptor)
{
    ASSERT(checkThread());
    SafePointScope scope(BlinkGC::HeapPointersOnStack);
    {
        MutexLocker locker(m_heap->threadAttachMutex());
        m_interruptors.append(std::move(interruptor));
    }
}

void ThreadState::registerStaticPersistentNode(PersistentNode* node, PersistentClearCallback callback)
{
#if defined(LEAK_SANITIZER)
    if (m_disabledStaticPersistentsRegistration)
        return;
#endif

    ASSERT(!m_staticPersistents.contains(node));
    m_staticPersistents.add(node, callback);
}

void ThreadState::releaseStaticPersistentNodes()
{
    HashMap<PersistentNode*, ThreadState::PersistentClearCallback> staticPersistents;
    staticPersistents.swap(m_staticPersistents);

    PersistentRegion* persistentRegion = getPersistentRegion();
    for (const auto& it : staticPersistents)
        persistentRegion->releasePersistentNode(it.key, it.value);
}

void ThreadState::freePersistentNode(PersistentNode* persistentNode)
{
    PersistentRegion* persistentRegion = getPersistentRegion();
    persistentRegion->freePersistentNode(persistentNode);
    // Do not allow static persistents to be freed before
    // they're all released in releaseStaticPersistentNodes().
    //
    // There's no fundamental reason why this couldn't be supported,
    // but no known use for it.
    ASSERT(!m_staticPersistents.contains(persistentNode));
}

#if defined(LEAK_SANITIZER)
void ThreadState::enterStaticReferenceRegistrationDisabledScope()
{
    m_disabledStaticPersistentsRegistration++;
}

void ThreadState::leaveStaticReferenceRegistrationDisabledScope()
{
    ASSERT(m_disabledStaticPersistentsRegistration);
    m_disabledStaticPersistentsRegistration--;
}
#endif

void ThreadState::lockThreadAttachMutex()
{
    m_heap->threadAttachMutex().lock();
}

void ThreadState::unlockThreadAttachMutex()
{
    m_heap->threadAttachMutex().unlock();
}

void ThreadState::invokePreFinalizers()
{
    ASSERT(checkThread());
    ASSERT(!sweepForbidden());
    TRACE_EVENT0("blink_gc", "ThreadState::invokePreFinalizers");

    double startTime = WTF::currentTimeMS();
    if (!m_orderedPreFinalizers.isEmpty()) {
        SweepForbiddenScope sweepForbidden(this);
        ScriptForbiddenIfMainThreadScope scriptForbidden;

        // Call the prefinalizers in the opposite order to their registration.
        //
        // The prefinalizer callback wrapper returns |true| when its associated
        // object is unreachable garbage and the prefinalizer callback has run.
        // The registered prefinalizer entry must then be removed and deleted.
        //
        auto it = --m_orderedPreFinalizers.end();
        bool done;
        do {
            auto entry = it;
            done = it == m_orderedPreFinalizers.begin();
            if (!done)
                --it;
            if ((entry->second)(entry->first))
                m_orderedPreFinalizers.remove(entry);
        } while (!done);
    }
    if (isMainThread()) {
        double timeForInvokingPreFinalizers = WTF::currentTimeMS() - startTime;
        DEFINE_STATIC_LOCAL(CustomCountHistogram, preFinalizersHistogram, ("BlinkGC.TimeForInvokingPreFinalizers", 1, 10 * 1000, 50));
        preFinalizersHistogram.count(timeForInvokingPreFinalizers);
    }
}

void ThreadState::clearArenaAges()
{
    memset(m_arenaAges, 0, sizeof(size_t) * BlinkGC::NumberOfArenas);
    memset(m_likelyToBePromptlyFreed.get(), 0, sizeof(int) * likelyToBePromptlyFreedArraySize);
    m_currentArenaAges = 0;
}

int ThreadState::arenaIndexOfVectorArenaLeastRecentlyExpanded(int beginArenaIndex, int endArenaIndex)
{
    size_t minArenaAge = m_arenaAges[beginArenaIndex];
    int arenaIndexWithMinArenaAge = beginArenaIndex;
    for (int arenaIndex = beginArenaIndex + 1; arenaIndex <= endArenaIndex; arenaIndex++) {
        if (m_arenaAges[arenaIndex] < minArenaAge) {
            minArenaAge = m_arenaAges[arenaIndex];
            arenaIndexWithMinArenaAge = arenaIndex;
        }
    }
    ASSERT(isVectorArenaIndex(arenaIndexWithMinArenaAge));
    return arenaIndexWithMinArenaAge;
}

BaseArena* ThreadState::expandedVectorBackingArena(size_t gcInfoIndex)
{
    ASSERT(checkThread());
    size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
    --m_likelyToBePromptlyFreed[entryIndex];
    int arenaIndex = m_vectorBackingArenaIndex;
    m_arenaAges[arenaIndex] = ++m_currentArenaAges;
    m_vectorBackingArenaIndex = arenaIndexOfVectorArenaLeastRecentlyExpanded(BlinkGC::Vector1ArenaIndex, BlinkGC::Vector4ArenaIndex);
    return m_arenas[arenaIndex];
}

void ThreadState::allocationPointAdjusted(int arenaIndex)
{
    m_arenaAges[arenaIndex] = ++m_currentArenaAges;
    if (m_vectorBackingArenaIndex == arenaIndex)
        m_vectorBackingArenaIndex = arenaIndexOfVectorArenaLeastRecentlyExpanded(BlinkGC::Vector1ArenaIndex, BlinkGC::Vector4ArenaIndex);
}

void ThreadState::promptlyFreed(size_t gcInfoIndex)
{
    ASSERT(checkThread());
    size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
    // See the comment in vectorBackingArena() for why this is +3.
    m_likelyToBePromptlyFreed[entryIndex] += 3;
}

void ThreadState::takeSnapshot(SnapshotType type)
{
    ASSERT(isInGC());

    // 0 is used as index for freelist entries. Objects are indexed 1 to
    // gcInfoIndex.
    GCSnapshotInfo info(GCInfoTable::gcInfoIndex() + 1);
    String threadDumpName = String::format("blink_gc/thread_%lu", static_cast<unsigned long>(m_thread));
    const String heapsDumpName = threadDumpName + "/heaps";
    const String classesDumpName = threadDumpName + "/classes";

    int numberOfHeapsReported = 0;
#define SNAPSHOT_HEAP(ArenaType)                                                                \
    {                                                                                          \
        numberOfHeapsReported++;                                                               \
        switch (type) {                                                                        \
        case SnapshotType::HeapSnapshot:                                                       \
            m_arenas[BlinkGC::ArenaType##ArenaIndex]->takeSnapshot(heapsDumpName + "/" #ArenaType, info);   \
            break;                                                                             \
        case SnapshotType::FreelistSnapshot:                                                   \
            m_arenas[BlinkGC::ArenaType##ArenaIndex]->takeFreelistSnapshot(heapsDumpName + "/" #ArenaType); \
            break;                                                                             \
        default:                                                                               \
            ASSERT_NOT_REACHED();                                                              \
        }                                                                                      \
    }

    SNAPSHOT_HEAP(NormalPage1);
    SNAPSHOT_HEAP(NormalPage2);
    SNAPSHOT_HEAP(NormalPage3);
    SNAPSHOT_HEAP(NormalPage4);
    SNAPSHOT_HEAP(EagerSweep);
    SNAPSHOT_HEAP(Vector1);
    SNAPSHOT_HEAP(Vector2);
    SNAPSHOT_HEAP(Vector3);
    SNAPSHOT_HEAP(Vector4);
    SNAPSHOT_HEAP(InlineVector);
    SNAPSHOT_HEAP(HashTable);
    SNAPSHOT_HEAP(LargeObject);
    FOR_EACH_TYPED_ARENA(SNAPSHOT_HEAP);

    ASSERT(numberOfHeapsReported == BlinkGC::NumberOfArenas);

#undef SNAPSHOT_HEAP

    if (type == SnapshotType::FreelistSnapshot)
        return;

    size_t totalLiveCount = 0;
    size_t totalDeadCount = 0;
    size_t totalLiveSize = 0;
    size_t totalDeadSize = 0;
    for (size_t gcInfoIndex = 1; gcInfoIndex <= GCInfoTable::gcInfoIndex(); ++gcInfoIndex) {
        totalLiveCount += info.liveCount[gcInfoIndex];
        totalDeadCount += info.deadCount[gcInfoIndex];
        totalLiveSize += info.liveSize[gcInfoIndex];
        totalDeadSize += info.deadSize[gcInfoIndex];
    }

    base::trace_event::MemoryAllocatorDump* threadDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(threadDumpName);
    threadDump->AddScalar("live_count", "objects", totalLiveCount);
    threadDump->AddScalar("dead_count", "objects", totalDeadCount);
    threadDump->AddScalar("live_size", "bytes", totalLiveSize);
    threadDump->AddScalar("dead_size", "bytes", totalDeadSize);

    base::trace_event::MemoryAllocatorDump* heapsDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(heapsDumpName);
    base::trace_event::MemoryAllocatorDump* classesDump = BlinkGCMemoryDumpProvider::instance()->createMemoryAllocatorDumpForCurrentGC(classesDumpName);
    BlinkGCMemoryDumpProvider::instance()->currentProcessMemoryDump()->AddOwnershipEdge(classesDump->guid(), heapsDump->guid());
}

} // namespace blink
