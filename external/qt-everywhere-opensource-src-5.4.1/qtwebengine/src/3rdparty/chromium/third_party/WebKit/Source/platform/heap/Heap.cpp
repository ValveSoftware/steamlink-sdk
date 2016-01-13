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

#include "config.h"
#include "platform/heap/Heap.h"

#include "platform/TraceEvent.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/PassOwnPtr.h"
#if ENABLE(GC_TRACING)
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include <stdio.h>
#include <utility>
#endif

#if OS(POSIX)
#include <sys/mman.h>
#include <unistd.h>
#elif OS(WIN)
#include <windows.h>
#endif

namespace WebCore {

#if ENABLE(GC_TRACING)
static String classOf(const void* object)
{
    const GCInfo* gcInfo = Heap::findGCInfo(reinterpret_cast<Address>(const_cast<void*>(object)));
    if (gcInfo)
        return gcInfo->m_className;

    return "unknown";
}
#endif

static bool vTableInitialized(void* objectPointer)
{
    return !!(*reinterpret_cast<Address*>(objectPointer));
}

#if OS(WIN)
static bool IsPowerOf2(size_t power)
{
    return !((power - 1) & power);
}
#endif

static Address roundToBlinkPageBoundary(void* base)
{
    return reinterpret_cast<Address>((reinterpret_cast<uintptr_t>(base) + blinkPageOffsetMask) & blinkPageBaseMask);
}

static size_t roundToOsPageSize(size_t size)
{
    return (size + osPageSize() - 1) & ~(osPageSize() - 1);
}

size_t osPageSize()
{
#if OS(POSIX)
    static const size_t pageSize = getpagesize();
#else
    static size_t pageSize = 0;
    if (!pageSize) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        ASSERT(IsPowerOf2(pageSize));
    }
#endif
    return pageSize;
}

class MemoryRegion {
public:
    MemoryRegion(Address base, size_t size)
        : m_base(base)
        , m_size(size)
    {
        ASSERT(size > 0);
    }

    bool contains(Address addr) const
    {
        return m_base <= addr && addr < (m_base + m_size);
    }


    bool contains(const MemoryRegion& other) const
    {
        return contains(other.m_base) && contains(other.m_base + other.m_size - 1);
    }

    void release()
    {
#if OS(POSIX)
        int err = munmap(m_base, m_size);
        RELEASE_ASSERT(!err);
#else
        bool success = VirtualFree(m_base, 0, MEM_RELEASE);
        RELEASE_ASSERT(success);
#endif
    }

    WARN_UNUSED_RETURN bool commit()
    {
        ASSERT(Heap::heapDoesNotContainCacheIsEmpty());
#if OS(POSIX)
        int err = mprotect(m_base, m_size, PROT_READ | PROT_WRITE);
        if (!err) {
            madvise(m_base, m_size, MADV_NORMAL);
            return true;
        }
        return false;
#else
        void* result = VirtualAlloc(m_base, m_size, MEM_COMMIT, PAGE_READWRITE);
        return !!result;
#endif
    }

    void decommit()
    {
#if OS(POSIX)
        int err = mprotect(m_base, m_size, PROT_NONE);
        RELEASE_ASSERT(!err);
        // FIXME: Consider using MADV_FREE on MacOS.
        madvise(m_base, m_size, MADV_DONTNEED);
#else
        bool success = VirtualFree(m_base, m_size, MEM_DECOMMIT);
        RELEASE_ASSERT(success);
#endif
    }

    Address base() const { return m_base; }
    size_t size() const { return m_size; }

private:
    Address m_base;
    size_t m_size;
};

// Representation of the memory used for a Blink heap page.
//
// The representation keeps track of two memory regions:
//
// 1. The virtual memory reserved from the sytem in order to be able
//    to free all the virtual memory reserved on destruction.
//
// 2. The writable memory (a sub-region of the reserved virtual
//    memory region) that is used for the actual heap page payload.
//
// Guard pages are created before and after the writable memory.
class PageMemory {
public:
    ~PageMemory()
    {
        __lsan_unregister_root_region(m_writable.base(), m_writable.size());
        m_reserved.release();
    }

    bool commit() WARN_UNUSED_RETURN { return m_writable.commit(); }
    void decommit() { m_writable.decommit(); }

    Address writableStart() { return m_writable.base(); }

    // Allocate a virtual address space for the blink page with the
    // following layout:
    //
    //    [ guard os page | ... payload ... | guard os page ]
    //    ^---{ aligned to blink page size }
    //
    static PageMemory* allocate(size_t payloadSize)
    {
        ASSERT(payloadSize > 0);

        // Virtual memory allocation routines operate in OS page sizes.
        // Round up the requested size to nearest os page size.
        payloadSize = roundToOsPageSize(payloadSize);

        // Overallocate by blinkPageSize and 2 times OS page size to
        // ensure a chunk of memory which is blinkPageSize aligned and
        // has a system page before and after to use for guarding. We
        // unmap the excess memory before returning.
        size_t allocationSize = payloadSize + 2 * osPageSize() + blinkPageSize;

        ASSERT(Heap::heapDoesNotContainCacheIsEmpty());
#if OS(POSIX)
        Address base = static_cast<Address>(mmap(0, allocationSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
        RELEASE_ASSERT(base != MAP_FAILED);

        Address end = base + allocationSize;
        Address alignedBase = roundToBlinkPageBoundary(base);
        Address payloadBase = alignedBase + osPageSize();
        Address payloadEnd = payloadBase + payloadSize;
        Address blinkPageEnd = payloadEnd + osPageSize();

        // If the allocate memory was not blink page aligned release
        // the memory before the aligned address.
        if (alignedBase != base)
            MemoryRegion(base, alignedBase - base).release();

        // Create guard pages by decommiting an OS page before and
        // after the payload.
        MemoryRegion(alignedBase, osPageSize()).decommit();
        MemoryRegion(payloadEnd, osPageSize()).decommit();

        // Free the additional memory at the end of the page if any.
        if (blinkPageEnd < end)
            MemoryRegion(blinkPageEnd, end - blinkPageEnd).release();

        return new PageMemory(MemoryRegion(alignedBase, blinkPageEnd - alignedBase), MemoryRegion(payloadBase, payloadSize));
#else
        Address base = 0;
        Address alignedBase = 0;

        // On Windows it is impossible to partially release a region
        // of memory allocated by VirtualAlloc. To avoid wasting
        // virtual address space we attempt to release a large region
        // of memory returned as a whole and then allocate an aligned
        // region inside this larger region.
        for (int attempt = 0; attempt < 3; attempt++) {
            base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
            RELEASE_ASSERT(base);
            VirtualFree(base, 0, MEM_RELEASE);

            alignedBase = roundToBlinkPageBoundary(base);
            base = static_cast<Address>(VirtualAlloc(alignedBase, payloadSize + 2 * osPageSize(), MEM_RESERVE, PAGE_NOACCESS));
            if (base) {
                RELEASE_ASSERT(base == alignedBase);
                allocationSize = payloadSize + 2 * osPageSize();
                break;
            }
        }

        if (!base) {
            // We failed to avoid wasting virtual address space after
            // several attempts.
            base = static_cast<Address>(VirtualAlloc(0, allocationSize, MEM_RESERVE, PAGE_NOACCESS));
            RELEASE_ASSERT(base);

            // FIXME: If base is by accident blink page size aligned
            // here then we can create two pages out of reserved
            // space. Do this.
            alignedBase = roundToBlinkPageBoundary(base);
        }

        Address payloadBase = alignedBase + osPageSize();
        PageMemory* storage = new PageMemory(MemoryRegion(base, allocationSize), MemoryRegion(payloadBase, payloadSize));
        bool res = storage->commit();
        RELEASE_ASSERT(res);
        return storage;
#endif
    }

private:
    PageMemory(const MemoryRegion& reserved, const MemoryRegion& writable)
        : m_reserved(reserved)
        , m_writable(writable)
    {
        ASSERT(reserved.contains(writable));

        // Register the writable area of the memory as part of the LSan root set.
        // Only the writable area is mapped and can contain C++ objects. Those
        // C++ objects can contain pointers to objects outside of the heap and
        // should therefore be part of the LSan root set.
        __lsan_register_root_region(m_writable.base(), m_writable.size());
    }

    MemoryRegion m_reserved;
    MemoryRegion m_writable;
};

class GCScope {
public:
    explicit GCScope(ThreadState::StackState stackState)
        : m_state(ThreadState::current())
        , m_safePointScope(stackState)
        , m_parkedAllThreads(false)
    {
        TRACE_EVENT0("Blink", "Heap::GCScope");
        const char* samplingState = TRACE_EVENT_GET_SAMPLING_STATE();
        if (m_state->isMainThread())
            TRACE_EVENT_SET_SAMPLING_STATE("Blink", "BlinkGCWaiting");

        m_state->checkThread();

        // FIXME: in an unlikely coincidence that two threads decide
        // to collect garbage at the same time, avoid doing two GCs in
        // a row.
        RELEASE_ASSERT(!m_state->isInGC());
        RELEASE_ASSERT(!m_state->isSweepInProgress());
        if (LIKELY(ThreadState::stopThreads())) {
            m_parkedAllThreads = true;
            m_state->enterGC();
        }
        if (m_state->isMainThread())
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(samplingState);
    }

    bool allThreadsParked() { return m_parkedAllThreads; }

    ~GCScope()
    {
        // Only cleanup if we parked all threads in which case the GC happened
        // and we need to resume the other threads.
        if (LIKELY(m_parkedAllThreads)) {
            m_state->leaveGC();
            ASSERT(!m_state->isInGC());
            ThreadState::resumeThreads();
        }
    }

private:
    ThreadState* m_state;
    ThreadState::SafePointScope m_safePointScope;
    bool m_parkedAllThreads; // False if we fail to park all threads
};

NO_SANITIZE_ADDRESS
bool HeapObjectHeader::isMarked() const
{
    checkHeader();
    return m_size & markBitMask;
}

NO_SANITIZE_ADDRESS
void HeapObjectHeader::unmark()
{
    checkHeader();
    m_size &= ~markBitMask;
}

NO_SANITIZE_ADDRESS
bool HeapObjectHeader::hasDebugMark() const
{
    checkHeader();
    return m_size & debugBitMask;
}

NO_SANITIZE_ADDRESS
void HeapObjectHeader::clearDebugMark()
{
    checkHeader();
    m_size &= ~debugBitMask;
}

NO_SANITIZE_ADDRESS
void HeapObjectHeader::setDebugMark()
{
    checkHeader();
    m_size |= debugBitMask;
}

#ifndef NDEBUG
NO_SANITIZE_ADDRESS
void HeapObjectHeader::zapMagic()
{
    m_magic = zappedMagic;
}
#endif

HeapObjectHeader* HeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(addr - objectHeaderSize);
    return header;
}

void HeapObjectHeader::finalize(const GCInfo* gcInfo, Address object, size_t objectSize)
{
    ASSERT(gcInfo);
    if (gcInfo->hasFinalizer()) {
        gcInfo->m_finalize(object);
    }
#if !defined(NDEBUG) || defined(LEAK_SANITIZER)
    // Zap freed memory with a recognizable zap value in debug mode.
    // Also zap when using leak sanitizer because the heap is used as
    // a root region for lsan and therefore pointers in unreachable
    // memory could hide leaks.
    for (size_t i = 0; i < objectSize; i++)
        object[i] = finalizedZapValue;
#endif
    // Zap the primary vTable entry (secondary vTable entries are not zapped)
    *(reinterpret_cast<uintptr_t*>(object)) = zappedVTable;
}

NO_SANITIZE_ADDRESS
void FinalizedHeapObjectHeader::finalize()
{
    HeapObjectHeader::finalize(m_gcInfo, payload(), payloadSize());
}

template<typename Header>
void LargeHeapObject<Header>::unmark()
{
    return heapObjectHeader()->unmark();
}

template<typename Header>
bool LargeHeapObject<Header>::isMarked()
{
    return heapObjectHeader()->isMarked();
}

template<typename Header>
void LargeHeapObject<Header>::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    if (!objectContains(address))
        return;
#if ENABLE(GC_TRACING)
    visitor->setHostInfo(&address, "stack");
#endif
    mark(visitor);
}

template<>
void LargeHeapObject<FinalizedHeapObjectHeader>::mark(Visitor* visitor)
{
    if (heapObjectHeader()->hasVTable() && !vTableInitialized(payload()))
        visitor->markConservatively(heapObjectHeader());
    else
        visitor->mark(heapObjectHeader(), heapObjectHeader()->traceCallback());
}

template<>
void LargeHeapObject<HeapObjectHeader>::mark(Visitor* visitor)
{
    ASSERT(gcInfo());
    if (gcInfo()->hasVTable() && !vTableInitialized(payload()))
        visitor->markConservatively(heapObjectHeader());
    else
        visitor->mark(heapObjectHeader(), gcInfo()->m_trace);
}

template<>
void LargeHeapObject<FinalizedHeapObjectHeader>::finalize()
{
    heapObjectHeader()->finalize();
}

template<>
void LargeHeapObject<HeapObjectHeader>::finalize()
{
    ASSERT(gcInfo());
    HeapObjectHeader::finalize(gcInfo(), payload(), payloadSize());
}

FinalizedHeapObjectHeader* FinalizedHeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    FinalizedHeapObjectHeader* header =
        reinterpret_cast<FinalizedHeapObjectHeader*>(addr - finalizedHeaderSize);
    return header;
}

template<typename Header>
ThreadHeap<Header>::ThreadHeap(ThreadState* state)
    : m_currentAllocationPoint(0)
    , m_remainingAllocationSize(0)
    , m_firstPage(0)
    , m_firstLargeHeapObject(0)
    , m_biggestFreeListIndex(0)
    , m_threadState(state)
    , m_pagePool(0)
{
    clearFreeLists();
}

template<typename Header>
ThreadHeap<Header>::~ThreadHeap()
{
    clearFreeLists();
    if (!ThreadState::current()->isMainThread())
        assertEmpty();
    deletePages();
}

template<typename Header>
Address ThreadHeap<Header>::outOfLineAllocate(size_t size, const GCInfo* gcInfo)
{
    size_t allocationSize = allocationSizeFromSize(size);
    if (threadState()->shouldGC()) {
        if (threadState()->shouldForceConservativeGC())
            Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        else
            threadState()->setGCRequested();
    }
    ensureCurrentAllocation(allocationSize, gcInfo);
    return allocate(size, gcInfo);
}

template<typename Header>
bool ThreadHeap<Header>::allocateFromFreeList(size_t minSize)
{
    size_t bucketSize = 1 << m_biggestFreeListIndex;
    int i = m_biggestFreeListIndex;
    for (; i > 0; i--, bucketSize >>= 1) {
        if (bucketSize < minSize)
            break;
        FreeListEntry* entry = m_freeLists[i];
        if (entry) {
            m_biggestFreeListIndex = i;
            entry->unlink(&m_freeLists[i]);
            setAllocationPoint(entry->address(), entry->size());
            ASSERT(currentAllocationPoint() && remainingAllocationSize() >= minSize);
            return true;
        }
    }
    m_biggestFreeListIndex = i;
    return false;
}

template<typename Header>
void ThreadHeap<Header>::ensureCurrentAllocation(size_t minSize, const GCInfo* gcInfo)
{
    ASSERT(minSize >= allocationGranularity);
    if (remainingAllocationSize() >= minSize)
        return;

    if (remainingAllocationSize() > 0)
        addToFreeList(currentAllocationPoint(), remainingAllocationSize());
    if (allocateFromFreeList(minSize))
        return;
    addPageToHeap(gcInfo);
    bool success = allocateFromFreeList(minSize);
    RELEASE_ASSERT(success);
}

template<typename Header>
BaseHeapPage* ThreadHeap<Header>::heapPageFromAddress(Address address)
{
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        if (page->contains(address))
            return page;
    }
    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current; current = current->next()) {
        // Check that large pages are blinkPageSize aligned (modulo the
        // osPageSize for the guard page).
        ASSERT(reinterpret_cast<Address>(current) - osPageSize() == roundToBlinkPageStart(reinterpret_cast<Address>(current)));
        if (current->contains(address))
            return current;
    }
    return 0;
}

#if ENABLE(GC_TRACING)
template<typename Header>
const GCInfo* ThreadHeap<Header>::findGCInfoOfLargeHeapObject(Address address)
{
    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current; current = current->next()) {
        if (current->contains(address))
            return current->gcInfo();
    }
    return 0;
}
#endif

template<typename Header>
void ThreadHeap<Header>::addToFreeList(Address address, size_t size)
{
    ASSERT(heapPageFromAddress(address));
    ASSERT(heapPageFromAddress(address + size - 1));
    ASSERT(size < blinkPagePayloadSize());
    // The free list entries are only pointer aligned (but when we allocate
    // from them we are 8 byte aligned due to the header size).
    ASSERT(!((reinterpret_cast<uintptr_t>(address) + sizeof(Header)) & allocationMask));
    ASSERT(!(size & allocationMask));
    ASAN_POISON_MEMORY_REGION(address, size);
    FreeListEntry* entry;
    if (size < sizeof(*entry)) {
        // Create a dummy header with only a size and freelist bit set.
        ASSERT(size >= sizeof(BasicObjectHeader));
        // Free list encode the size to mark the lost memory as freelist memory.
        new (NotNull, address) BasicObjectHeader(BasicObjectHeader::freeListEncodedSize(size));
        // This memory gets lost. Sweeping can reclaim it.
        return;
    }
    entry = new (NotNull, address) FreeListEntry(size);
#if defined(ADDRESS_SANITIZER)
    // For ASAN we don't add the entry to the free lists until the asanDeferMemoryReuseCount
    // reaches zero. However we always add entire pages to ensure that adding a new page will
    // increase the allocation space.
    if (HeapPage<Header>::payloadSize() != size && !entry->shouldAddToFreeList())
        return;
#endif
    int index = bucketIndexForSize(size);
    entry->link(&m_freeLists[index]);
    if (index > m_biggestFreeListIndex)
        m_biggestFreeListIndex = index;
}

template<typename Header>
Address ThreadHeap<Header>::allocateLargeObject(size_t size, const GCInfo* gcInfo)
{
    // Caller already added space for object header and rounded up to allocation alignment
    ASSERT(!(size & allocationMask));

    size_t allocationSize = sizeof(LargeHeapObject<Header>) + size;

    // Ensure that there is enough space for alignment. If the header
    // is not a multiple of 8 bytes we will allocate an extra
    // headerPadding<Header> bytes to ensure it 8 byte aligned.
    allocationSize += headerPadding<Header>();

    // If ASAN is supported we add allocationGranularity bytes to the allocated space and
    // poison that to detect overflows
#if defined(ADDRESS_SANITIZER)
    allocationSize += allocationGranularity;
#endif
    if (threadState()->shouldGC())
        threadState()->setGCRequested();
    Heap::flushHeapDoesNotContainCache();
    PageMemory* pageMemory = PageMemory::allocate(allocationSize);
    Address largeObjectAddress = pageMemory->writableStart();
    Address headerAddress = largeObjectAddress + sizeof(LargeHeapObject<Header>) + headerPadding<Header>();
    memset(headerAddress, 0, size);
    Header* header = new (NotNull, headerAddress) Header(size, gcInfo);
    Address result = headerAddress + sizeof(*header);
    ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));
    LargeHeapObject<Header>* largeObject = new (largeObjectAddress) LargeHeapObject<Header>(pageMemory, gcInfo, threadState());

    // Poison the object header and allocationGranularity bytes after the object
    ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
    ASAN_POISON_MEMORY_REGION(largeObject->address() + largeObject->size(), allocationGranularity);
    largeObject->link(&m_firstLargeHeapObject);
    stats().increaseAllocatedSpace(largeObject->size());
    stats().increaseObjectSpace(largeObject->payloadSize());
    return result;
}

template<typename Header>
void ThreadHeap<Header>::freeLargeObject(LargeHeapObject<Header>* object, LargeHeapObject<Header>** previousNext)
{
    flushHeapContainsCache();
    object->unlink(previousNext);
    object->finalize();

    // Unpoison the object header and allocationGranularity bytes after the
    // object before freeing.
    ASAN_UNPOISON_MEMORY_REGION(object->heapObjectHeader(), sizeof(Header));
    ASAN_UNPOISON_MEMORY_REGION(object->address() + object->size(), allocationGranularity);
    delete object->storage();
}

template<>
void ThreadHeap<FinalizedHeapObjectHeader>::addPageToHeap(const GCInfo* gcInfo)
{
    // When adding a page to the ThreadHeap using FinalizedHeapObjectHeaders the GCInfo on
    // the heap should be unused (ie. 0).
    allocatePage(0);
}

template<>
void ThreadHeap<HeapObjectHeader>::addPageToHeap(const GCInfo* gcInfo)
{
    // When adding a page to the ThreadHeap using HeapObjectHeaders store the GCInfo on the heap
    // since it is the same for all objects
    ASSERT(gcInfo);
    allocatePage(gcInfo);
}

template<typename Header>
void ThreadHeap<Header>::clearPagePool()
{
    while (takePageFromPool()) { }
}

template<typename Header>
PageMemory* ThreadHeap<Header>::takePageFromPool()
{
    Heap::flushHeapDoesNotContainCache();
    while (PagePoolEntry* entry = m_pagePool) {
        m_pagePool = entry->next();
        PageMemory* storage = entry->storage();
        delete entry;

        if (storage->commit())
            return storage;

        // Failed to commit pooled storage. Release it.
        delete storage;
    }

    return 0;
}

template<typename Header>
void ThreadHeap<Header>::addPageToPool(HeapPage<Header>* unused)
{
    flushHeapContainsCache();
    PageMemory* storage = unused->storage();
    PagePoolEntry* entry = new PagePoolEntry(storage, m_pagePool);
    m_pagePool = entry;
    storage->decommit();
}

template<typename Header>
void ThreadHeap<Header>::allocatePage(const GCInfo* gcInfo)
{
    Heap::flushHeapDoesNotContainCache();
    PageMemory* pageMemory = takePageFromPool();
    if (!pageMemory) {
        pageMemory = PageMemory::allocate(blinkPagePayloadSize());
        RELEASE_ASSERT(pageMemory);
    }
    HeapPage<Header>* page = new (pageMemory->writableStart()) HeapPage<Header>(pageMemory, this, gcInfo);
    // FIXME: Oilpan: Linking new pages into the front of the list is
    // crucial when performing allocations during finalization because
    // it ensures that those pages are not swept in the current GC
    // round. We should create a separate page list for that to
    // separate out the pages allocated during finalization clearly
    // from the pages currently being swept.
    page->link(&m_firstPage);
    addToFreeList(page->payload(), HeapPage<Header>::payloadSize());
}

#ifndef NDEBUG
template<typename Header>
void ThreadHeap<Header>::getScannedStats(HeapStats& scannedStats)
{
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next())
        page->getStats(scannedStats);
    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current; current = current->next())
        current->getStats(scannedStats);
}
#endif

// STRICT_ASAN_FINALIZATION_CHECKING turns on poisoning of all objects during
// sweeping to catch cases where dead objects touch eachother. This is not
// turned on by default because it also triggers for cases that are safe.
// Examples of such safe cases are context life cycle observers and timers
// embedded in garbage collected objects.
#define STRICT_ASAN_FINALIZATION_CHECKING 0

template<typename Header>
void ThreadHeap<Header>::sweep()
{
    ASSERT(isConsistentForGC());
#if defined(ADDRESS_SANITIZER) && STRICT_ASAN_FINALIZATION_CHECKING
    // When using ASAN do a pre-sweep where all unmarked objects are poisoned before
    // calling their finalizer methods. This can catch the cases where one objects
    // finalizer tries to modify another object as part of finalization.
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next())
        page->poisonUnmarkedObjects();
#endif
    HeapPage<Header>* page = m_firstPage;
    HeapPage<Header>** previous = &m_firstPage;
    bool pagesRemoved = false;
    while (page) {
        if (page->isEmpty()) {
            flushHeapContainsCache();
            HeapPage<Header>* unused = page;
            page = page->next();
            HeapPage<Header>::unlink(unused, previous);
            pagesRemoved = true;
        } else {
            page->sweep();
            previous = &page->m_next;
            page = page->next();
        }
    }
    if (pagesRemoved)
        flushHeapContainsCache();

    LargeHeapObject<Header>** previousNext = &m_firstLargeHeapObject;
    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current;) {
        if (current->isMarked()) {
            stats().increaseAllocatedSpace(current->size());
            stats().increaseObjectSpace(current->payloadSize());
            current->unmark();
            previousNext = &current->m_next;
            current = current->next();
        } else {
            LargeHeapObject<Header>* next = current->next();
            freeLargeObject(current, previousNext);
            current = next;
        }
    }
}

template<typename Header>
void ThreadHeap<Header>::assertEmpty()
{
    // No allocations are permitted. The thread is exiting.
    NoAllocationScope<AnyThread> noAllocation;
    makeConsistentForGC();
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next()) {
        Address end = page->end();
        Address headerAddress;
        for (headerAddress = page->payload(); headerAddress < end; ) {
            BasicObjectHeader* basicHeader = reinterpret_cast<BasicObjectHeader*>(headerAddress);
            ASSERT(basicHeader->size() < blinkPagePayloadSize());
            // A live object is potentially a dangling pointer from
            // some root. Treat that as a bug. Unfortunately, it is
            // hard to reliably check in the presence of conservative
            // stack scanning. Something could be conservatively kept
            // alive because a non-pointer on another thread's stack
            // is treated as a pointer into the heap.
            //
            // FIXME: This assert can currently trigger in cases where
            // worker shutdown does not get enough precise GCs to get
            // all objects removed from the worker heap. There are two
            // issues: 1) conservative GCs keeping objects alive, and
            // 2) long chains of RefPtrs/Persistents that require more
            // GCs to get everything cleaned up. Maybe we can keep
            // threads alive until their heaps become empty instead of
            // forcing the threads to die immediately?
            ASSERT(Heap::lastGCWasConservative() || basicHeader->isFree());
            if (basicHeader->isFree())
                addToFreeList(headerAddress, basicHeader->size());
            headerAddress += basicHeader->size();
        }
        ASSERT(headerAddress == end);
    }

    ASSERT(Heap::lastGCWasConservative() || !m_firstLargeHeapObject);
}

template<typename Header>
bool ThreadHeap<Header>::isConsistentForGC()
{
    for (size_t i = 0; i < blinkPageSizeLog2; i++) {
        if (m_freeLists[i])
            return false;
    }
    return !ownsNonEmptyAllocationArea();
}

template<typename Header>
void ThreadHeap<Header>::makeConsistentForGC()
{
    if (ownsNonEmptyAllocationArea())
        addToFreeList(currentAllocationPoint(), remainingAllocationSize());
    setAllocationPoint(0, 0);
    clearFreeLists();
}

template<typename Header>
void ThreadHeap<Header>::clearMarks()
{
    ASSERT(isConsistentForGC());
    for (HeapPage<Header>* page = m_firstPage; page; page = page->next())
        page->clearMarks();
    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current; current = current->next())
        current->unmark();
}

template<typename Header>
void ThreadHeap<Header>::deletePages()
{
    flushHeapContainsCache();
    // Add all pages in the pool to the heap's list of pages before deleting
    clearPagePool();

    for (HeapPage<Header>* page = m_firstPage; page; ) {
        HeapPage<Header>* dead = page;
        page = page->next();
        PageMemory* storage = dead->storage();
        dead->~HeapPage();
        delete storage;
    }
    m_firstPage = 0;

    for (LargeHeapObject<Header>* current = m_firstLargeHeapObject; current;) {
        LargeHeapObject<Header>* dead = current;
        current = current->next();
        PageMemory* storage = dead->storage();
        dead->~LargeHeapObject();
        delete storage;
    }
    m_firstLargeHeapObject = 0;
}

template<typename Header>
void ThreadHeap<Header>::clearFreeLists()
{
    for (size_t i = 0; i < blinkPageSizeLog2; i++)
        m_freeLists[i] = 0;
}

int BaseHeap::bucketIndexForSize(size_t size)
{
    ASSERT(size > 0);
    int index = -1;
    while (size) {
        size >>= 1;
        index++;
    }
    return index;
}

template<typename Header>
HeapPage<Header>::HeapPage(PageMemory* storage, ThreadHeap<Header>* heap, const GCInfo* gcInfo)
    : BaseHeapPage(storage, gcInfo, heap->threadState())
    , m_next(0)
    , m_heap(heap)
{
    COMPILE_ASSERT(!(sizeof(HeapPage<Header>) & allocationMask), page_header_incorrectly_aligned);
    m_objectStartBitMapComputed = false;
    ASSERT(isPageHeaderAddress(reinterpret_cast<Address>(this)));
    heap->stats().increaseAllocatedSpace(blinkPageSize);
}

template<typename Header>
void HeapPage<Header>::link(HeapPage** prevNext)
{
    m_next = *prevNext;
    *prevNext = this;
}

template<typename Header>
void HeapPage<Header>::unlink(HeapPage* unused, HeapPage** prevNext)
{
    *prevNext = unused->m_next;
    unused->heap()->addPageToPool(unused);
}

template<typename Header>
void HeapPage<Header>::getStats(HeapStats& stats)
{
    stats.increaseAllocatedSpace(blinkPageSize);
    Address headerAddress = payload();
    ASSERT(headerAddress != end());
    do {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        if (!header->isFree())
            stats.increaseObjectSpace(header->payloadSize());
        ASSERT(header->size() < blinkPagePayloadSize());
        headerAddress += header->size();
        ASSERT(headerAddress <= end());
    } while (headerAddress < end());
}

template<typename Header>
bool HeapPage<Header>::isEmpty()
{
    BasicObjectHeader* header = reinterpret_cast<BasicObjectHeader*>(payload());
    return header->isFree() && (header->size() == payloadSize());
}

template<typename Header>
void HeapPage<Header>::sweep()
{
    clearObjectStartBitMap();
    heap()->stats().increaseAllocatedSpace(blinkPageSize);
    Address startOfGap = payload();
    for (Address headerAddress = startOfGap; headerAddress < end(); ) {
        BasicObjectHeader* basicHeader = reinterpret_cast<BasicObjectHeader*>(headerAddress);
        ASSERT(basicHeader->size() < blinkPagePayloadSize());

        if (basicHeader->isFree()) {
            headerAddress += basicHeader->size();
            continue;
        }
        // At this point we know this is a valid object of type Header
        Header* header = static_cast<Header*>(basicHeader);

        if (!header->isMarked()) {
            // For ASAN we unpoison the specific object when calling the finalizer and
            // poison it again when done to allow the object's own finalizer to operate
            // on the object, but not have other finalizers be allowed to access it.
            ASAN_UNPOISON_MEMORY_REGION(header->payload(), header->payloadSize());
            finalize(header);
            ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
            headerAddress += header->size();
            continue;
        }

        if (startOfGap != headerAddress)
            heap()->addToFreeList(startOfGap, headerAddress - startOfGap);
        header->unmark();
        headerAddress += header->size();
        heap()->stats().increaseObjectSpace(header->payloadSize());
        startOfGap = headerAddress;
    }
    if (startOfGap != end())
        heap()->addToFreeList(startOfGap, end() - startOfGap);
}

template<typename Header>
void HeapPage<Header>::clearMarks()
{
    for (Address headerAddress = payload(); headerAddress < end();) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());
        if (!header->isFree())
            header->unmark();
        headerAddress += header->size();
    }
}

template<typename Header>
void HeapPage<Header>::populateObjectStartBitMap()
{
    memset(&m_objectStartBitMap, 0, objectStartBitMapSize);
    Address start = payload();
    for (Address headerAddress = start; headerAddress < end();) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        size_t objectOffset = headerAddress - start;
        ASSERT(!(objectOffset & allocationMask));
        size_t objectStartNumber = objectOffset / allocationGranularity;
        size_t mapIndex = objectStartNumber / 8;
        ASSERT(mapIndex < objectStartBitMapSize);
        m_objectStartBitMap[mapIndex] |= (1 << (objectStartNumber & 7));
        headerAddress += header->size();
        ASSERT(headerAddress <= end());
    }
    m_objectStartBitMapComputed = true;
}

template<typename Header>
void HeapPage<Header>::clearObjectStartBitMap()
{
    m_objectStartBitMapComputed = false;
}

static int numberOfLeadingZeroes(uint8_t byte)
{
    if (!byte)
        return 8;
    int result = 0;
    if (byte <= 0x0F) {
        result += 4;
        byte = byte << 4;
    }
    if (byte <= 0x3F) {
        result += 2;
        byte = byte << 2;
    }
    if (byte <= 0x7F)
        result++;
    return result;
}

template<typename Header>
Header* HeapPage<Header>::findHeaderFromAddress(Address address)
{
    if (address < payload())
        return 0;
    if (!isObjectStartBitMapComputed())
        populateObjectStartBitMap();
    size_t objectOffset = address - payload();
    size_t objectStartNumber = objectOffset / allocationGranularity;
    size_t mapIndex = objectStartNumber / 8;
    ASSERT(mapIndex < objectStartBitMapSize);
    size_t bit = objectStartNumber & 7;
    uint8_t byte = m_objectStartBitMap[mapIndex] & ((1 << (bit + 1)) - 1);
    while (!byte) {
        ASSERT(mapIndex > 0);
        byte = m_objectStartBitMap[--mapIndex];
    }
    int leadingZeroes = numberOfLeadingZeroes(byte);
    objectStartNumber = (mapIndex * 8) + 7 - leadingZeroes;
    objectOffset = objectStartNumber * allocationGranularity;
    Address objectAddress = objectOffset + payload();
    Header* header = reinterpret_cast<Header*>(objectAddress);
    if (header->isFree())
        return 0;
    return header;
}

template<typename Header>
void HeapPage<Header>::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(contains(address));
    Header* header = findHeaderFromAddress(address);
    if (!header)
        return;

#if ENABLE(GC_TRACING)
    visitor->setHostInfo(&address, "stack");
#endif
    if (hasVTable(header) && !vTableInitialized(header->payload()))
        visitor->markConservatively(header);
    else
        visitor->mark(header, traceCallback(header));
}

#if ENABLE(GC_TRACING)
template<typename Header>
const GCInfo* HeapPage<Header>::findGCInfo(Address address)
{
    if (address < payload())
        return 0;

    if (gcInfo()) // for non FinalizedObjectHeader
        return gcInfo();

    Header* header = findHeaderFromAddress(address);
    if (!header)
        return 0;

    return header->gcInfo();
}
#endif

#if defined(ADDRESS_SANITIZER)
template<typename Header>
void HeapPage<Header>::poisonUnmarkedObjects()
{
    for (Address headerAddress = payload(); headerAddress < end(); ) {
        Header* header = reinterpret_cast<Header*>(headerAddress);
        ASSERT(header->size() < blinkPagePayloadSize());

        if (!header->isFree() && !header->isMarked())
            ASAN_POISON_MEMORY_REGION(header->payload(), header->payloadSize());
        headerAddress += header->size();
    }
}
#endif

template<>
inline void HeapPage<FinalizedHeapObjectHeader>::finalize(FinalizedHeapObjectHeader* header)
{
    header->finalize();
}

template<>
inline void HeapPage<HeapObjectHeader>::finalize(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    HeapObjectHeader::finalize(gcInfo(), header->payload(), header->payloadSize());
}

template<>
inline TraceCallback HeapPage<HeapObjectHeader>::traceCallback(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    return gcInfo()->m_trace;
}

template<>
inline TraceCallback HeapPage<FinalizedHeapObjectHeader>::traceCallback(FinalizedHeapObjectHeader* header)
{
    return header->traceCallback();
}

template<>
inline bool HeapPage<HeapObjectHeader>::hasVTable(HeapObjectHeader* header)
{
    ASSERT(gcInfo());
    return gcInfo()->hasVTable();
}

template<>
inline bool HeapPage<FinalizedHeapObjectHeader>::hasVTable(FinalizedHeapObjectHeader* header)
{
    return header->hasVTable();
}

template<typename Header>
void LargeHeapObject<Header>::getStats(HeapStats& stats)
{
    stats.increaseAllocatedSpace(size());
    stats.increaseObjectSpace(payloadSize());
}

template<typename Entry>
void HeapExtentCache<Entry>::flush()
{
    if (m_hasEntries) {
        for (int i = 0; i < numberOfEntries; i++)
            m_entries[i] = Entry();
        m_hasEntries = false;
    }
}

template<typename Entry>
size_t HeapExtentCache<Entry>::hash(Address address)
{
    size_t value = (reinterpret_cast<size_t>(address) >> blinkPageSizeLog2);
    value ^= value >> numberOfEntriesLog2;
    value ^= value >> (numberOfEntriesLog2 * 2);
    value &= numberOfEntries - 1;
    return value & ~1; // Returns only even number.
}

template<typename Entry>
typename Entry::LookupResult HeapExtentCache<Entry>::lookup(Address address)
{
    size_t index = hash(address);
    ASSERT(!(index & 1));
    Address cachePage = roundToBlinkPageStart(address);
    if (m_entries[index].address() == cachePage)
        return m_entries[index].result();
    if (m_entries[index + 1].address() == cachePage)
        return m_entries[index + 1].result();
    return 0;
}

template<typename Entry>
void HeapExtentCache<Entry>::addEntry(Address address, typename Entry::LookupResult entry)
{
    m_hasEntries = true;
    size_t index = hash(address);
    ASSERT(!(index & 1));
    Address cachePage = roundToBlinkPageStart(address);
    m_entries[index + 1] = m_entries[index];
    m_entries[index] = Entry(cachePage, entry);
}

// These should not be needed, but it seems impossible to persuade clang to
// instantiate the template functions and export them from a shared library, so
// we add these in the non-templated subclass, which does not have that issue.
void HeapContainsCache::addEntry(Address address, BaseHeapPage* page)
{
    HeapExtentCache<PositiveEntry>::addEntry(address, page);
}

BaseHeapPage* HeapContainsCache::lookup(Address address)
{
    return HeapExtentCache<PositiveEntry>::lookup(address);
}

void Heap::flushHeapDoesNotContainCache()
{
    s_heapDoesNotContainCache->flush();
}

void CallbackStack::init(CallbackStack** first)
{
    // The stacks are chained, so we start by setting this to null as terminator.
    *first = 0;
    *first = new CallbackStack(first);
}

void CallbackStack::shutdown(CallbackStack** first)
{
    CallbackStack* next;
    for (CallbackStack* current = *first; current; current = next) {
        next = current->m_next;
        delete current;
    }
    *first = 0;
}

CallbackStack::~CallbackStack()
{
#ifndef NDEBUG
    clearUnused();
#endif
}

void CallbackStack::clearUnused()
{
    ASSERT(m_current == &(m_buffer[0]));
    for (size_t i = 0; i < bufferSize; i++)
        m_buffer[i] = Item(0, 0);
}

void CallbackStack::assertIsEmpty()
{
    ASSERT(m_current == &(m_buffer[0]));
    ASSERT(!m_next);
}

bool CallbackStack::popAndInvokeCallback(CallbackStack** first, Visitor* visitor)
{
    if (m_current == &(m_buffer[0])) {
        if (!m_next) {
#ifndef NDEBUG
            clearUnused();
#endif
            return false;
        }
        CallbackStack* nextStack = m_next;
        *first = nextStack;
        delete this;
        return nextStack->popAndInvokeCallback(first, visitor);
    }
    Item* item = --m_current;

    VisitorCallback callback = item->callback();
#if ENABLE(GC_TRACING)
    if (ThreadState::isAnyThreadInGC()) // weak-processing will also use popAndInvokeCallback
        visitor->setHostInfo(item->object(), classOf(item->object()));
#endif
    callback(visitor, item->object());

    return true;
}

class MarkingVisitor : public Visitor {
public:
#if ENABLE(GC_TRACING)
    typedef HashSet<uintptr_t> LiveObjectSet;
    typedef HashMap<String, LiveObjectSet> LiveObjectMap;
    typedef HashMap<uintptr_t, std::pair<uintptr_t, String> > ObjectGraph;
#endif

    inline void visitHeader(HeapObjectHeader* header, const void* objectPointer, TraceCallback callback)
    {
        ASSERT(header);
        ASSERT(objectPointer);
        if (header->isMarked())
            return;
        header->mark();
#if ENABLE(GC_TRACING)
        MutexLocker locker(objectGraphMutex());
        String className(classOf(objectPointer));
        {
            LiveObjectMap::AddResult result = currentlyLive().add(className, LiveObjectSet());
            result.storedValue->value.add(reinterpret_cast<uintptr_t>(objectPointer));
        }
        ObjectGraph::AddResult result = objectGraph().add(reinterpret_cast<uintptr_t>(objectPointer), std::make_pair(reinterpret_cast<uintptr_t>(m_hostObject), m_hostName));
        ASSERT(result.isNewEntry);
        // fprintf(stderr, "%s[%p] -> %s[%p]\n", m_hostName.ascii().data(), m_hostObject, className.ascii().data(), objectPointer);
#endif
        if (callback)
            Heap::pushTraceCallback(const_cast<void*>(objectPointer), callback);
    }

    virtual void mark(HeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitHeader(header, header->payload(), callback);
    }

    virtual void mark(FinalizedHeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitHeader(header, header->payload(), callback);
    }

    virtual void mark(const void* objectPointer, TraceCallback callback) OVERRIDE
    {
        if (!objectPointer)
            return;
        FinalizedHeapObjectHeader* header = FinalizedHeapObjectHeader::fromPayload(objectPointer);
        visitHeader(header, header->payload(), callback);
    }


    inline void visitConservatively(HeapObjectHeader* header, void* objectPointer, size_t objectSize)
    {
        ASSERT(header);
        ASSERT(objectPointer);
        if (header->isMarked())
            return;
        header->mark();

        // Scan through the object's fields and visit them conservatively.
        Address* objectFields = reinterpret_cast<Address*>(objectPointer);
        for (size_t i = 0; i < objectSize / sizeof(Address); ++i)
            Heap::checkAndMarkPointer(this, objectFields[i]);
    }

    virtual void markConservatively(HeapObjectHeader* header)
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitConservatively(header, header->payload(), header->payloadSize());
    }

    virtual void markConservatively(FinalizedHeapObjectHeader* header)
    {
        // We need both the HeapObjectHeader and FinalizedHeapObjectHeader
        // version to correctly find the payload.
        visitConservatively(header, header->payload(), header->payloadSize());
    }

    virtual void registerWeakMembers(const void* closure, const void* containingObject, WeakPointerCallback callback) OVERRIDE
    {
        Heap::pushWeakObjectPointerCallback(const_cast<void*>(closure), const_cast<void*>(containingObject), callback);
    }

    virtual bool isMarked(const void* objectPointer) OVERRIDE
    {
        return FinalizedHeapObjectHeader::fromPayload(objectPointer)->isMarked();
    }

    // This macro defines the necessary visitor methods for typed heaps
#define DEFINE_VISITOR_METHODS(Type)                                              \
    virtual void mark(const Type* objectPointer, TraceCallback callback) OVERRIDE \
    {                                                                             \
        if (!objectPointer)                                                       \
            return;                                                               \
        HeapObjectHeader* header =                                                \
            HeapObjectHeader::fromPayload(objectPointer);                         \
        visitHeader(header, header->payload(), callback);                         \
    }                                                                             \
    virtual bool isMarked(const Type* objectPointer) OVERRIDE                     \
    {                                                                             \
        return HeapObjectHeader::fromPayload(objectPointer)->isMarked();          \
    }

    FOR_EACH_TYPED_HEAP(DEFINE_VISITOR_METHODS)
#undef DEFINE_VISITOR_METHODS

#if ENABLE(GC_TRACING)
    void reportStats()
    {
        fprintf(stderr, "\n---------- AFTER MARKING -------------------\n");
        for (LiveObjectMap::iterator it = currentlyLive().begin(), end = currentlyLive().end(); it != end; ++it) {
            fprintf(stderr, "%s %u", it->key.ascii().data(), it->value.size());

            if (it->key == "WebCore::Document")
                reportStillAlive(it->value, previouslyLive().get(it->key));

            fprintf(stderr, "\n");
        }

        previouslyLive().swap(currentlyLive());
        currentlyLive().clear();

        for (HashSet<uintptr_t>::iterator it = objectsToFindPath().begin(), end = objectsToFindPath().end(); it != end; ++it) {
            dumpPathToObjectFromObjectGraph(objectGraph(), *it);
        }
    }

    static void reportStillAlive(LiveObjectSet current, LiveObjectSet previous)
    {
        int count = 0;

        fprintf(stderr, " [previously %u]", previous.size());
        for (LiveObjectSet::iterator it = current.begin(), end = current.end(); it != end; ++it) {
            if (previous.find(*it) == previous.end())
                continue;
            count++;
        }

        if (!count)
            return;

        fprintf(stderr, " {survived 2GCs %d: ", count);
        for (LiveObjectSet::iterator it = current.begin(), end = current.end(); it != end; ++it) {
            if (previous.find(*it) == previous.end())
                continue;
            fprintf(stderr, "%ld", *it);
            if (--count)
                fprintf(stderr, ", ");
        }
        ASSERT(!count);
        fprintf(stderr, "}");
    }

    static void dumpPathToObjectFromObjectGraph(const ObjectGraph& graph, uintptr_t target)
    {
        ObjectGraph::const_iterator it = graph.find(target);
        if (it == graph.end())
            return;
        fprintf(stderr, "Path to %lx of %s\n", target, classOf(reinterpret_cast<const void*>(target)).ascii().data());
        while (it != graph.end()) {
            fprintf(stderr, "<- %lx of %s\n", it->value.first, it->value.second.utf8().data());
            it = graph.find(it->value.first);
        }
        fprintf(stderr, "\n");
    }

    static void dumpPathToObjectOnNextGC(void* p)
    {
        objectsToFindPath().add(reinterpret_cast<uintptr_t>(p));
    }

    static Mutex& objectGraphMutex()
    {
        AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
        return mutex;
    }

    static LiveObjectMap& previouslyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static LiveObjectMap& currentlyLive()
    {
        DEFINE_STATIC_LOCAL(LiveObjectMap, map, ());
        return map;
    }

    static ObjectGraph& objectGraph()
    {
        DEFINE_STATIC_LOCAL(ObjectGraph, graph, ());
        return graph;
    }

    static HashSet<uintptr_t>& objectsToFindPath()
    {
        DEFINE_STATIC_LOCAL(HashSet<uintptr_t>, set, ());
        return set;
    }
#endif

protected:
    virtual void registerWeakCell(void** cell, WeakPointerCallback callback) OVERRIDE
    {
        Heap::pushWeakCellPointerCallback(cell, callback);
    }
};

void Heap::init()
{
    ThreadState::init();
    CallbackStack::init(&s_markingStack);
    CallbackStack::init(&s_weakCallbackStack);
    s_heapDoesNotContainCache = new HeapDoesNotContainCache();
    s_markingVisitor = new MarkingVisitor();
}

void Heap::shutdown()
{
    s_shutdownCalled = true;
    ThreadState::shutdownHeapIfNecessary();
}

void Heap::doShutdown()
{
    // We don't want to call doShutdown() twice.
    if (!s_markingVisitor)
        return;

    ASSERT(!ThreadState::isAnyThreadInGC());
    ASSERT(!ThreadState::attachedThreads().size());
    delete s_markingVisitor;
    s_markingVisitor = 0;
    delete s_heapDoesNotContainCache;
    s_heapDoesNotContainCache = 0;
    CallbackStack::shutdown(&s_weakCallbackStack);
    CallbackStack::shutdown(&s_markingStack);
    ThreadState::shutdown();
}

BaseHeapPage* Heap::contains(Address address)
{
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        BaseHeapPage* page = (*it)->contains(address);
        if (page)
            return page;
    }
    return 0;
}

Address Heap::checkAndMarkPointer(Visitor* visitor, Address address)
{
    ASSERT(ThreadState::isAnyThreadInGC());

#ifdef NDEBUG
    if (s_heapDoesNotContainCache->lookup(address))
        return 0;
#endif

    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        if ((*it)->checkAndMarkPointer(visitor, address)) {
            // Pointer was in a page of that thread. If it actually pointed
            // into an object then that object was found and marked.
            ASSERT(!s_heapDoesNotContainCache->lookup(address));
            s_lastGCWasConservative = true;
            return address;
        }
    }

#ifdef NDEBUG
    s_heapDoesNotContainCache->addEntry(address, true);
#else
    if (!s_heapDoesNotContainCache->lookup(address))
        s_heapDoesNotContainCache->addEntry(address, true);
#endif
    return 0;
}

#if ENABLE(GC_TRACING)
const GCInfo* Heap::findGCInfo(Address address)
{
    return ThreadState::findGCInfoFromAllThreads(address);
}

void Heap::dumpPathToObjectOnNextGC(void* p)
{
    static_cast<MarkingVisitor*>(s_markingVisitor)->dumpPathToObjectOnNextGC(p);
}

String Heap::createBacktraceString()
{
    int framesToShow = 3;
    int stackFrameSize = 16;
    ASSERT(stackFrameSize >= framesToShow);
    typedef void* FramePointer;
    FramePointer* stackFrame = static_cast<FramePointer*>(alloca(sizeof(FramePointer) * stackFrameSize));
    WTFGetBacktrace(stackFrame, &stackFrameSize);

    StringBuilder builder;
    builder.append("Persistent");
    bool didAppendFirstName = false;
    // Skip frames before/including "WebCore::Persistent".
    bool didSeePersistent = false;
    for (int i = 0; i < stackFrameSize && framesToShow > 0; ++i) {
        FrameToNameScope frameToName(stackFrame[i]);
        if (!frameToName.nullableName())
            continue;
        if (strstr(frameToName.nullableName(), "WebCore::Persistent")) {
            didSeePersistent = true;
            continue;
        }
        if (!didSeePersistent)
            continue;
        if (!didAppendFirstName) {
            didAppendFirstName = true;
            builder.append(" ... Backtrace:");
        }
        builder.append("\n\t");
        builder.append(frameToName.nullableName());
        --framesToShow;
    }
    return builder.toString().replace("WebCore::", "");
}
#endif

void Heap::pushTraceCallback(void* object, TraceCallback callback)
{
    ASSERT(Heap::contains(object));
    CallbackStack::Item* slot = s_markingStack->allocateEntry(&s_markingStack);
    *slot = CallbackStack::Item(object, callback);
}

bool Heap::popAndInvokeTraceCallback(Visitor* visitor)
{
    return s_markingStack->popAndInvokeCallback(&s_markingStack, visitor);
}

void Heap::pushWeakCellPointerCallback(void** cell, WeakPointerCallback callback)
{
    ASSERT(Heap::contains(cell));
    CallbackStack::Item* slot = s_weakCallbackStack->allocateEntry(&s_weakCallbackStack);
    *slot = CallbackStack::Item(cell, callback);
}

void Heap::pushWeakObjectPointerCallback(void* closure, void* object, WeakPointerCallback callback)
{
    ASSERT(Heap::contains(object));
    BaseHeapPage* heapPageForObject = reinterpret_cast<BaseHeapPage*>(pageHeaderAddress(reinterpret_cast<Address>(object)));
    ASSERT(Heap::contains(object) == heapPageForObject);
    ThreadState* state = heapPageForObject->threadState();
    state->pushWeakObjectPointerCallback(closure, callback);
}

bool Heap::popAndInvokeWeakPointerCallback(Visitor* visitor)
{
    return s_weakCallbackStack->popAndInvokeCallback(&s_weakCallbackStack, visitor);
}

void Heap::prepareForGC()
{
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->prepareForGC();
}

void Heap::collectGarbage(ThreadState::StackState stackState)
{
    ThreadState* state = ThreadState::current();
    state->clearGCRequested();

    GCScope gcScope(stackState);
    // Check if we successfully parked the other threads. If not we bail out of the GC.
    if (!gcScope.allThreadsParked()) {
        ThreadState::current()->setGCRequested();
        return;
    }

    s_lastGCWasConservative = false;

    TRACE_EVENT0("Blink", "Heap::collectGarbage");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BlinkGC");
    double timeStamp = WTF::currentTimeMS();
#if ENABLE(GC_TRACING)
    static_cast<MarkingVisitor*>(s_markingVisitor)->objectGraph().clear();
#endif

    // Disallow allocation during garbage collection (but not
    // during the finalization that happens when the gcScope is
    // torn down).
    NoAllocationScope<AnyThread> noAllocationScope;

    prepareForGC();

    ThreadState::visitRoots(s_markingVisitor);
    // Recursively mark all objects that are reachable from the roots.
    while (popAndInvokeTraceCallback(s_markingVisitor)) { }

    // Call weak callbacks on objects that may now be pointing to dead
    // objects.
    while (popAndInvokeWeakPointerCallback(s_markingVisitor)) { }

    // It is not permitted to trace pointers of live objects in the weak
    // callback phase, so the marking stack should still be empty here.
    s_markingStack->assertIsEmpty();

#if ENABLE(GC_TRACING)
    static_cast<MarkingVisitor*>(s_markingVisitor)->reportStats();
#endif

    if (blink::Platform::current()) {
        uint64_t objectSpaceSize;
        uint64_t allocatedSpaceSize;
        getHeapSpaceSize(&objectSpaceSize, &allocatedSpaceSize);
        blink::Platform::current()->histogramCustomCounts("BlinkGC.CollectGarbage", WTF::currentTimeMS() - timeStamp, 0, 10 * 1000, 50);
        blink::Platform::current()->histogramCustomCounts("BlinkGC.TotalObjectSpace", objectSpaceSize / 1024, 0, 4 * 1024 * 1024, 50);
        blink::Platform::current()->histogramCustomCounts("BlinkGC.TotalAllocatedSpace", allocatedSpaceSize / 1024, 0, 4 * 1024 * 1024, 50);
    }
}

void Heap::collectAllGarbage()
{
    // FIXME: oilpan: we should perform a single GC and everything
    // should die. Unfortunately it is not the case for all objects
    // because the hierarchy was not completely moved to the heap and
    // some heap allocated objects own objects that contain persistents
    // pointing to other heap allocated objects.
    for (int i = 0; i < 5; i++)
        collectGarbage(ThreadState::NoHeapPointersOnStack);
}

void Heap::setForcePreciseGCForTesting()
{
    ThreadState::current()->setForcePreciseGCForTesting(true);
}

void Heap::getHeapSpaceSize(uint64_t* objectSpaceSize, uint64_t* allocatedSpaceSize)
{
    *objectSpaceSize = 0;
    *allocatedSpaceSize = 0;
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    typedef ThreadState::AttachedThreadStateSet::iterator ThreadStateIterator;
    for (ThreadStateIterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        *objectSpaceSize += (*it)->stats().totalObjectSpace();
        *allocatedSpaceSize += (*it)->stats().totalAllocatedSpace();
    }
}

void Heap::getStats(HeapStats* stats)
{
    stats->clear();
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    typedef ThreadState::AttachedThreadStateSet::iterator ThreadStateIterator;
    for (ThreadStateIterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        HeapStats temp;
        (*it)->getStats(temp);
        stats->add(&temp);
    }
}

bool Heap::isConsistentForGC()
{
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it) {
        if (!(*it)->isConsistentForGC())
            return false;
    }
    return true;
}

void Heap::makeConsistentForGC()
{
    ASSERT(ThreadState::isAnyThreadInGC());
    ThreadState::AttachedThreadStateSet& threads = ThreadState::attachedThreads();
    for (ThreadState::AttachedThreadStateSet::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
        (*it)->makeConsistentForGC();
}

// Force template instantiations for the types that we need.
template class HeapPage<FinalizedHeapObjectHeader>;
template class HeapPage<HeapObjectHeader>;
template class ThreadHeap<FinalizedHeapObjectHeader>;
template class ThreadHeap<HeapObjectHeader>;

Visitor* Heap::s_markingVisitor;
CallbackStack* Heap::s_markingStack;
CallbackStack* Heap::s_weakCallbackStack;
HeapDoesNotContainCache* Heap::s_heapDoesNotContainCache;
bool Heap::s_shutdownCalled = false;
bool Heap::s_lastGCWasConservative = false;
}
