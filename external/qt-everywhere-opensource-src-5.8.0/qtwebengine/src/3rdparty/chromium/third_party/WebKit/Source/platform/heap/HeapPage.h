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

#ifndef HeapPage_h
#define HeapPage_h

#include "base/trace_event/memory_allocator_dump.h"
#include "platform/PlatformExport.h"
#include "platform/heap/BlinkGC.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/Forward.h"
#include "wtf/allocator/PageAllocator.h"
#include <stdint.h>

namespace blink {

const size_t blinkPageSizeLog2 = 17;
const size_t blinkPageSize = 1 << blinkPageSizeLog2;
const size_t blinkPageOffsetMask = blinkPageSize - 1;
const size_t blinkPageBaseMask = ~blinkPageOffsetMask;

// We allocate pages at random addresses but in groups of
// blinkPagesPerRegion at a given random address. We group pages to
// not spread out too much over the address space which would blow
// away the page tables and lead to bad performance.
const size_t blinkPagesPerRegion = 10;

// TODO(nya): Replace this with something like #if ENABLE_NACL.
#if 0
// NaCl's system page size is 64 KB. This causes a problem in Oilpan's heap
// layout because Oilpan allocates two guard pages for each blink page
// (whose size is 128 KB). So we don't use guard pages in NaCl.
const size_t blinkGuardPageSize = 0;
#else
const size_t blinkGuardPageSize = WTF::kSystemPageSize;
#endif

// Double precision floats are more efficient when 8 byte aligned, so we 8 byte
// align all allocations even on 32 bit.
const size_t allocationGranularity = 8;
const size_t allocationMask = allocationGranularity - 1;
const size_t objectStartBitMapSize = (blinkPageSize + ((8 * allocationGranularity) - 1)) / (8 * allocationGranularity);
const size_t reservedForObjectBitMap = ((objectStartBitMapSize + allocationMask) & ~allocationMask);
const size_t maxHeapObjectSizeLog2 = 27;
const size_t maxHeapObjectSize = 1 << maxHeapObjectSizeLog2;
const size_t largeObjectSizeThreshold = blinkPageSize / 2;

// A zap value used for freed memory that is allowed to be added to the free
// list in the next addToFreeList().
const uint8_t reuseAllowedZapValue = 0x2a;
// A zap value used for freed memory that is forbidden to be added to the free
// list in the next addToFreeList().
const uint8_t reuseForbiddenZapValue = 0x2c;

// In non-production builds, memory is zapped when it's freed. The zapped
// memory is zeroed out when the memory is reused in ThreadHeap::allocateObject().
// In production builds, memory is not zapped (for performance). The memory
// is just zeroed out when it is added to the free list.
#if defined(MEMORY_SANITIZER)
// TODO(kojii): We actually need __msan_poison/unpoison here, but it'll be
// added later.
#define SET_MEMORY_INACCESSIBLE(address, size) \
    FreeList::zapFreedMemory(address, size);
#define SET_MEMORY_ACCESSIBLE(address, size) \
    memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size) \
    ASAN_UNPOISON_MEMORY_REGION(address, size); \
    FreeList::checkFreedMemoryIsZapped(address, size); \
    ASAN_POISON_MEMORY_REGION(address, size)
#elif ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define SET_MEMORY_INACCESSIBLE(address, size) \
    FreeList::zapFreedMemory(address, size);   \
    ASAN_POISON_MEMORY_REGION(address, size)
#define SET_MEMORY_ACCESSIBLE(address, size) \
    ASAN_UNPOISON_MEMORY_REGION(address, size); \
    memset((address), 0, (size))
#define CHECK_MEMORY_INACCESSIBLE(address, size) \
    ASAN_UNPOISON_MEMORY_REGION(address, size); \
    FreeList::checkFreedMemoryIsZapped(address, size); \
    ASAN_POISON_MEMORY_REGION(address, size)
#else
#define SET_MEMORY_INACCESSIBLE(address, size) memset((address), 0, (size))
#define SET_MEMORY_ACCESSIBLE(address, size) do { } while (false)
#define CHECK_MEMORY_INACCESSIBLE(address, size) do { } while (false)
#endif

#if !ENABLE(ASSERT) && CPU(64BIT)
#define USE_4BYTE_HEADER_PADDING 1
#else
#define USE_4BYTE_HEADER_PADDING 0
#endif

class CallbackStack;
class FreePagePool;
class NormalPageArena;
class OrphanedPagePool;
class PageMemory;
class PageMemoryRegion;
class WebMemoryAllocatorDump;

// HeapObjectHeader is 4 byte (32 bit) that has the following layout:
//
// | gcInfoIndex (14 bit) | DOM mark bit (1 bit) | size (14 bit) | dead bit (1 bit) | freed bit (1 bit) | mark bit (1 bit) |
//
// - For non-large objects, 14 bit is enough for |size| because the blink
//   page size is 2^17 byte and each object is guaranteed to be aligned with
//   2^3 byte.
// - For large objects, |size| is 0. The actual size of a large object is
//   stored in LargeObjectPage::m_payloadSize.
// - 1 bit used to mark DOM trees for V8.
// - 14 bit is enough for gcInfoIndex because there are less than 2^14 types
//   in Blink.
const size_t headerWrapperMarkBitMask = 1u << 17;
const size_t headerGCInfoIndexShift = 18;
const size_t headerGCInfoIndexMask = (static_cast<size_t>((1 << 14) - 1)) << headerGCInfoIndexShift;
const size_t headerSizeMask = (static_cast<size_t>((1 << 14) - 1)) << 3;
const size_t headerMarkBitMask = 1;
const size_t headerFreedBitMask = 2;
// The dead bit is used for objects that have gone through a GC marking, but did
// not get swept before a new GC started. In that case we set the dead bit on
// objects that were not marked in the previous GC to ensure we are not tracing
// them via a conservatively found pointer. Tracing dead objects could lead to
// tracing of already finalized objects in another thread's heap which is a
// use-after-free situation.
const size_t headerDeadBitMask = 4;
// On free-list entries we reuse the dead bit to distinguish a normal free-list
// entry from one that has been promptly freed.
const size_t headerPromptlyFreedBitMask = headerFreedBitMask | headerDeadBitMask;
const size_t largeObjectSizeInHeader = 0;
const size_t gcInfoIndexForFreeListHeader = 0;
const size_t nonLargeObjectPageSizeMax = 1 << 17;

static_assert(nonLargeObjectPageSizeMax >= blinkPageSize, "max size supported by HeapObjectHeader must at least be blinkPageSize");

class PLATFORM_EXPORT HeapObjectHeader {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    // If gcInfoIndex is 0, this header is interpreted as a free list header.
    NO_SANITIZE_ADDRESS
    HeapObjectHeader(size_t size, size_t gcInfoIndex)
    {
#if ENABLE(ASSERT)
        m_magic = magic;
#endif
        // sizeof(HeapObjectHeader) must be equal to or smaller than
        // allocationGranurarity, because HeapObjectHeader is used as a header
        // for an freed entry.  Given that the smallest entry size is
        // allocationGranurarity, HeapObjectHeader must fit into the size.
        static_assert(sizeof(HeapObjectHeader) <= allocationGranularity, "size of HeapObjectHeader must be smaller than allocationGranularity");
#if CPU(64BIT)
        static_assert(sizeof(HeapObjectHeader) == 8, "size of HeapObjectHeader must be 8 byte aligned");
#endif

        ASSERT(gcInfoIndex < GCInfoTable::maxIndex);
        ASSERT(size < nonLargeObjectPageSizeMax);
        ASSERT(!(size & allocationMask));
        m_encoded = static_cast<uint32_t>((gcInfoIndex << headerGCInfoIndexShift) | size | (gcInfoIndex == gcInfoIndexForFreeListHeader ? headerFreedBitMask : 0));
    }

    NO_SANITIZE_ADDRESS
    bool isFree() const { return m_encoded & headerFreedBitMask; }
    NO_SANITIZE_ADDRESS
    bool isPromptlyFreed() const { return (m_encoded & headerPromptlyFreedBitMask) == headerPromptlyFreedBitMask; }
    NO_SANITIZE_ADDRESS
    void markPromptlyFreed() { m_encoded |= headerPromptlyFreedBitMask; }
    size_t size() const;

    NO_SANITIZE_ADDRESS
    size_t gcInfoIndex() const { return (m_encoded & headerGCInfoIndexMask) >> headerGCInfoIndexShift; }
    NO_SANITIZE_ADDRESS
    void setSize(size_t size)
    {
        ASSERT(size < nonLargeObjectPageSizeMax);
        m_encoded = static_cast<uint32_t>(size) | (m_encoded & ~headerSizeMask);
    }
    bool isWrapperHeaderMarked() const;
    void markWrapperHeader();
    void unmarkWrapperHeader();
    bool isMarked() const;
    void mark();
    void unmark();
    void markDead();
    bool isDead() const;

    Address payload();
    size_t payloadSize();
    Address payloadEnd();

#if ENABLE(ASSERT)
    bool checkHeader() const;
    // Zap magic number with a new magic number that means there was once an
    // object allocated here, but it was freed because nobody marked it during
    // GC.
    void zapMagic();
#endif

    void finalize(Address, size_t);
    static HeapObjectHeader* fromPayload(const void*);

    static const uint16_t magic = 0xfff1;
    static const uint16_t zappedMagic = 0x4321;

private:
    uint32_t m_encoded;
#if ENABLE(ASSERT)
    uint16_t m_magic;
#endif

    // In 64 bit architectures, we intentionally add 4 byte padding immediately
    // after the HeapObjectHeader. This is because:
    //
    // | HeapObjectHeader (4 byte) | padding (4 byte) | object payload (8 * n byte) |
    // ^8 byte aligned                                ^8 byte aligned
    //
    // is better than:
    //
    // | HeapObjectHeader (4 byte) | object payload (8 * n byte) | padding (4 byte) |
    // ^4 byte aligned             ^8 byte aligned               ^4 byte aligned
    //
    // since the former layout aligns both header and payload to 8 byte.
#if USE_4BYTE_HEADER_PADDING
public:
    uint32_t m_padding;
#endif
};

class FreeListEntry final : public HeapObjectHeader {
public:
    NO_SANITIZE_ADDRESS
    explicit FreeListEntry(size_t size)
        : HeapObjectHeader(size, gcInfoIndexForFreeListHeader)
        , m_next(nullptr)
    {
#if ENABLE(ASSERT)
        ASSERT(size >= sizeof(HeapObjectHeader));
        zapMagic();
#endif
    }

    Address getAddress() { return reinterpret_cast<Address>(this); }

    NO_SANITIZE_ADDRESS
    void unlink(FreeListEntry** prevNext)
    {
        *prevNext = m_next;
        m_next = nullptr;
    }

    NO_SANITIZE_ADDRESS
    void link(FreeListEntry** prevNext)
    {
        m_next = *prevNext;
        *prevNext = this;
    }

    NO_SANITIZE_ADDRESS
    FreeListEntry* next() const { return m_next; }

    NO_SANITIZE_ADDRESS
    void append(FreeListEntry* next)
    {
        ASSERT(!m_next);
        m_next = next;
    }

private:
    FreeListEntry* m_next;
};

// Blink heap pages are set up with a guard page before and after the payload.
inline size_t blinkPagePayloadSize()
{
    return blinkPageSize - 2 * blinkGuardPageSize;
}

// Blink heap pages are aligned to the Blink heap page size.
// Therefore, the start of a Blink page can be obtained by
// rounding down to the Blink page size.
inline Address roundToBlinkPageStart(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

inline Address roundToBlinkPageEnd(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address - 1) & blinkPageBaseMask) + blinkPageSize;
}

// Masks an address down to the enclosing blink page base address.
inline Address blinkPageAddress(Address address)
{
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address) & blinkPageBaseMask);
}

inline bool vTableInitialized(void* objectPointer)
{
    return !!(*reinterpret_cast<Address*>(objectPointer));
}

#if ENABLE(ASSERT)
// Sanity check for a page header address: the address of the page
// header should be OS page size away from being Blink page size
// aligned.
inline bool isPageHeaderAddress(Address address)
{
    return !((reinterpret_cast<uintptr_t>(address) & blinkPageOffsetMask) - blinkGuardPageSize);
}
#endif

// BasePage is a base class for NormalPage and LargeObjectPage.
//
// - NormalPage is a page whose size is |blinkPageSize|. NormalPage can contain
//   multiple objects in the page. An object whose size is smaller than
//   |largeObjectSizeThreshold| is stored in NormalPage.
//
// - LargeObjectPage is a page that contains only one object. The object size
//   is arbitrary. An object whose size is larger than |blinkPageSize| is stored
//   as a single project in LargeObjectPage.
//
// Note: An object whose size is between |largeObjectSizeThreshold| and
// |blinkPageSize| can go to either of NormalPage or LargeObjectPage.
class BasePage {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    BasePage(PageMemory*, BaseArena*);
    virtual ~BasePage() { }

    void link(BasePage** previousNext)
    {
        m_next = *previousNext;
        *previousNext = this;
    }
    void unlink(BasePage** previousNext)
    {
        *previousNext = m_next;
        m_next = nullptr;
    }
    BasePage* next() const { return m_next; }

    // virtual methods are slow. So performance-sensitive methods
    // should be defined as non-virtual methods on NormalPage and LargeObjectPage.
    // The following methods are not performance-sensitive.
    virtual size_t objectPayloadSizeForTesting() = 0;
    virtual bool isEmpty() = 0;
    virtual void removeFromHeap() = 0;
    virtual void sweep() = 0;
    virtual void makeConsistentForGC() = 0;
    virtual void makeConsistentForMutator() = 0;
    virtual void invalidateObjectStartBitmap() = 0;

#if defined(ADDRESS_SANITIZER)
    virtual void poisonUnmarkedObjects() = 0;
#endif
    // Check if the given address points to an object in this
    // heap page. If so, find the start of that object and mark it
    // using the given Visitor. Otherwise do nothing. The pointer must
    // be within the same aligned blinkPageSize as the this-pointer.
    //
    // This is used during conservative stack scanning to
    // conservatively mark all objects that could be referenced from
    // the stack.
    virtual void checkAndMarkPointer(Visitor*, Address) = 0;
    virtual void markOrphaned();

    class HeapSnapshotInfo {
        STACK_ALLOCATED();
    public:
        size_t freeCount = 0;
        size_t freeSize = 0;
    };

    virtual void takeSnapshot(base::trace_event::MemoryAllocatorDump*, ThreadState::GCSnapshotInfo&, HeapSnapshotInfo&) = 0;
#if ENABLE(ASSERT)
    virtual bool contains(Address) = 0;
#endif
    virtual size_t size() = 0;
    virtual bool isLargeObjectPage() { return false; }

    Address getAddress() { return reinterpret_cast<Address>(this); }
    PageMemory* storage() const { return m_storage; }
    BaseArena* arena() const { return m_arena; }
    bool orphaned() { return !m_arena; }
    bool terminating() { return m_terminating; }
    void setTerminating() { m_terminating = true; }

    // Returns true if this page has been swept by the ongoing lazy sweep.
    bool hasBeenSwept() const { return m_swept; }

    void markAsSwept()
    {
        ASSERT(!m_swept);
        m_swept = true;
    }

    void markAsUnswept()
    {
        ASSERT(m_swept);
        m_swept = false;
    }

private:
    PageMemory* m_storage;
    BaseArena* m_arena;
    BasePage* m_next;
    // Whether the page is part of a terminating thread or not.
    bool m_terminating;

    // Track the sweeping state of a page. Set to true once
    // the lazy sweep completes has processed it.
    //
    // Set to false at the start of a sweep, true  upon completion
    // of lazy sweeping.
    bool m_swept;
    friend class BaseArena;
};

class NormalPage final : public BasePage {
public:
    NormalPage(PageMemory*, BaseArena*);

    Address payload()
    {
        return getAddress() + pageHeaderSize();
    }
    size_t payloadSize()
    {
        return (blinkPagePayloadSize() - pageHeaderSize()) & ~allocationMask;
    }
    Address payloadEnd() { return payload() + payloadSize(); }
    bool containedInObjectPayload(Address address)
    {
        return payload() <= address && address < payloadEnd();
    }

    size_t objectPayloadSizeForTesting() override;
    bool isEmpty() override;
    void removeFromHeap() override;
    void sweep() override;
    void makeConsistentForGC() override;
    void makeConsistentForMutator() override;
    void invalidateObjectStartBitmap() override { m_objectStartBitMapComputed = false; }
#if defined(ADDRESS_SANITIZER)
    void poisonUnmarkedObjects() override;
#endif
    void checkAndMarkPointer(Visitor*, Address) override;
    void markOrphaned() override;

    void takeSnapshot(base::trace_event::MemoryAllocatorDump*, ThreadState::GCSnapshotInfo&, HeapSnapshotInfo&) override;
#if ENABLE(ASSERT)
    // Returns true for the whole blinkPageSize page that the page is on, even
    // for the header, and the unmapped guard page at the start. That ensures
    // the result can be used to populate the negative page cache.
    bool contains(Address) override;
#endif
    size_t size() override { return blinkPageSize; }
    static size_t pageHeaderSize()
    {
        // Compute the amount of padding we have to add to a header to make
        // the size of the header plus the padding a multiple of 8 bytes.
        size_t paddingSize = (sizeof(NormalPage) + allocationGranularity - (sizeof(HeapObjectHeader) % allocationGranularity)) % allocationGranularity;
        return sizeof(NormalPage) + paddingSize;
    }

    inline NormalPageArena* arenaForNormalPage() const;

private:
    HeapObjectHeader* findHeaderFromAddress(Address);
    void populateObjectStartBitMap();

    bool m_objectStartBitMapComputed;
    uint8_t m_objectStartBitMap[reservedForObjectBitMap];
};

// Large allocations are allocated as separate objects and linked in a list.
//
// In order to use the same memory allocation routines for everything allocated
// in the heap, large objects are considered heap pages containing only one
// object.
class LargeObjectPage final : public BasePage {
public:
    LargeObjectPage(PageMemory*, BaseArena*, size_t);

    Address payload() { return heapObjectHeader()->payload(); }
    size_t payloadSize() { return m_payloadSize; }
    Address payloadEnd() { return payload() + payloadSize(); }
    bool containedInObjectPayload(Address address)
    {
        return payload() <= address && address < payloadEnd();
    }

    size_t objectPayloadSizeForTesting() override;
    bool isEmpty() override;
    void removeFromHeap() override;
    void sweep() override;
    void makeConsistentForGC() override;
    void makeConsistentForMutator() override;
    void invalidateObjectStartBitmap() override { }
#if defined(ADDRESS_SANITIZER)
    void poisonUnmarkedObjects() override;
#endif
    void checkAndMarkPointer(Visitor*, Address) override;
    void markOrphaned() override;

    void takeSnapshot(base::trace_event::MemoryAllocatorDump*, ThreadState::GCSnapshotInfo&, HeapSnapshotInfo&) override;
#if ENABLE(ASSERT)
    // Returns true for any address that is on one of the pages that this
    // large object uses. That ensures that we can use a negative result to
    // populate the negative page cache.
    bool contains(Address) override;
#endif
    virtual size_t size()
    {
        return pageHeaderSize() +  sizeof(HeapObjectHeader) + m_payloadSize;
    }
    static size_t pageHeaderSize()
    {
        // Compute the amount of padding we have to add to a header to make
        // the size of the header plus the padding a multiple of 8 bytes.
        size_t paddingSize = (sizeof(LargeObjectPage) + allocationGranularity - (sizeof(HeapObjectHeader) % allocationGranularity)) % allocationGranularity;
        return sizeof(LargeObjectPage) + paddingSize;
    }
    bool isLargeObjectPage() override { return true; }

    HeapObjectHeader* heapObjectHeader()
    {
        Address headerAddress = getAddress() + pageHeaderSize();
        return reinterpret_cast<HeapObjectHeader*>(headerAddress);
    }

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    void setIsVectorBackingPage() { m_isVectorBackingPage = true; }
    bool isVectorBackingPage() const { return m_isVectorBackingPage; }
#endif

private:

    size_t m_payloadSize;
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    bool m_isVectorBackingPage;
#endif
};

// A HeapDoesNotContainCache provides a fast way of taking an arbitrary
// pointer-sized word, and determining whether it cannot be interpreted as a
// pointer to an area that is managed by the garbage collected Blink heap.  This
// is a cache of 'pages' that have previously been determined to be wholly
// outside of the heap.  The size of these pages must be smaller than the
// allocation alignment of the heap pages.  We determine off-heap-ness by
// rounding down the pointer to the nearest page and looking up the page in the
// cache.  If there is a miss in the cache we can determine the status of the
// pointer precisely using the heap RegionTree.
//
// The HeapDoesNotContainCache is a negative cache, so it must be flushed when
// memory is added to the heap.
class HeapDoesNotContainCache {
    USING_FAST_MALLOC(HeapDoesNotContainCache);
public:
    HeapDoesNotContainCache()
        : m_hasEntries(false)
    {
        // Start by flushing the cache in a non-empty state to initialize all the cache entries.
        for (int i = 0; i < numberOfEntries; ++i)
            m_entries[i] = nullptr;
    }

    void flush();
    bool isEmpty() { return !m_hasEntries; }

    // Perform a lookup in the cache.
    //
    // If lookup returns false, the argument address was not found in
    // the cache and it is unknown if the address is in the Blink
    // heap.
    //
    // If lookup returns true, the argument address was found in the
    // cache which means the address is not in the heap.
    PLATFORM_EXPORT bool lookup(Address);

    // Add an entry to the cache.
    PLATFORM_EXPORT void addEntry(Address);

private:
    static const int numberOfEntriesLog2 = 12;
    static const int numberOfEntries = 1 << numberOfEntriesLog2;

    static size_t hash(Address);

    Address m_entries[numberOfEntries];
    bool m_hasEntries;
};

class FreeList {
    DISALLOW_NEW();
public:
    FreeList();

    void addToFreeList(Address, size_t);
    void clear();

    // Returns a bucket number for inserting a FreeListEntry of a given size.
    // All FreeListEntries in the given bucket, n, have size >= 2^n.
    static int bucketIndexForSize(size_t);

    // Returns true if the freelist snapshot is captured.
    bool takeSnapshot(const String& dumpBaseName);

#if ENABLE(ASSERT) || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
    static void zapFreedMemory(Address, size_t);
    static void checkFreedMemoryIsZapped(Address, size_t);
#endif

private:
    int m_biggestFreeListIndex;

    // All FreeListEntries in the nth list have size >= 2^n.
    FreeListEntry* m_freeLists[blinkPageSizeLog2];

    friend class NormalPageArena;
};

// Each thread has a number of thread arenas (e.g., Generic arenas,
// typed arenas for Node, arenas for collection backings etc)
// and BaseArena represents each thread arena.
//
// BaseArena is a parent class of NormalPageArena and LargeObjectArena.
// NormalPageArena represents a part of a heap that contains NormalPages
// and LargeObjectArena represents a part of a heap that contains
// LargeObjectPages.
class PLATFORM_EXPORT BaseArena {
    USING_FAST_MALLOC(BaseArena);
public:
    BaseArena(ThreadState*, int);
    virtual ~BaseArena();
    void cleanupPages();

    void takeSnapshot(const String& dumpBaseName, ThreadState::GCSnapshotInfo&);
#if ENABLE(ASSERT)
    BasePage* findPageFromAddress(Address);
#endif
    virtual void takeFreelistSnapshot(const String& dumpBaseName) { }
    virtual void clearFreeLists() { }
    void makeConsistentForGC();
    void makeConsistentForMutator();
#if ENABLE(ASSERT)
    virtual bool isConsistentForGC() = 0;
#endif
    size_t objectPayloadSizeForTesting();
    void prepareHeapForTermination();
    void prepareForSweep();
#if defined(ADDRESS_SANITIZER)
    void poisonArena();
#endif
    Address lazySweep(size_t, size_t gcInfoIndex);
    void sweepUnsweptPage();
    // Returns true if we have swept all pages within the deadline.
    // Returns false otherwise.
    bool lazySweepWithDeadline(double deadlineSeconds);
    void completeSweep();

    ThreadState* getThreadState() { return m_threadState; }
    int arenaIndex() const { return m_index; }

    Address allocateLargeObject(size_t allocationSize, size_t gcInfoIndex);

    bool willObjectBeLazilySwept(BasePage*, void*) const;

protected:
    BasePage* m_firstPage;
    BasePage* m_firstUnsweptPage;

private:
    virtual Address lazySweepPages(size_t, size_t gcInfoIndex) = 0;

    ThreadState* m_threadState;

    // Index into the page pools.  This is used to ensure that the pages of the
    // same type go into the correct page pool and thus avoid type confusion.
    int m_index;
};

class PLATFORM_EXPORT NormalPageArena final : public BaseArena {
public:
    NormalPageArena(ThreadState*, int);
    void addToFreeList(Address address, size_t size)
    {
        ASSERT(findPageFromAddress(address));
        ASSERT(findPageFromAddress(address + size - 1));
        m_freeList.addToFreeList(address, size);
    }
    void clearFreeLists() override;
#if ENABLE(ASSERT)
    bool isConsistentForGC() override;
    bool pagesToBeSweptContains(Address);
#endif
    void takeFreelistSnapshot(const String& dumpBaseName) override;

    Address allocateObject(size_t allocationSize, size_t gcInfoIndex);

    void freePage(NormalPage*);

    bool coalesce();
    void promptlyFreeObject(HeapObjectHeader*);
    bool expandObject(HeapObjectHeader*, size_t);
    bool shrinkObject(HeapObjectHeader*, size_t);
    void decreasePromptlyFreedSize(size_t size) { m_promptlyFreedSize -= size; }

    bool isObjectAllocatedAtAllocationPoint(HeapObjectHeader* header)
    {
        return header->payloadEnd() == m_currentAllocationPoint;
    }

    bool isLazySweeping() const { return m_isLazySweeping; }
    void setIsLazySweeping(bool flag) { m_isLazySweeping = flag; }

private:
    void allocatePage();
    Address outOfLineAllocate(size_t allocationSize, size_t gcInfoIndex);
    Address allocateFromFreeList(size_t, size_t gcInfoIndex);

    Address lazySweepPages(size_t, size_t gcInfoIndex) override;

    Address currentAllocationPoint() const { return m_currentAllocationPoint; }
    bool hasCurrentAllocationArea() const { return currentAllocationPoint() && remainingAllocationSize(); }
    void setAllocationPoint(Address, size_t);

    size_t remainingAllocationSize() const { return m_remainingAllocationSize; }
    void setRemainingAllocationSize(size_t);
    void updateRemainingAllocationSize();

    FreeList m_freeList;
    Address m_currentAllocationPoint;
    size_t m_remainingAllocationSize;
    size_t m_lastRemainingAllocationSize;

    // The size of promptly freed objects in the heap.
    size_t m_promptlyFreedSize;

    bool m_isLazySweeping;
};

class LargeObjectArena final : public BaseArena {
public:
    LargeObjectArena(ThreadState*, int);
    Address allocateLargeObjectPage(size_t, size_t gcInfoIndex);
    void freeLargeObjectPage(LargeObjectPage*);
#if ENABLE(ASSERT)
    bool isConsistentForGC() override { return true; }
#endif
private:
    Address doAllocateLargeObjectPage(size_t, size_t gcInfoIndex);
    Address lazySweepPages(size_t, size_t gcInfoIndex) override;
};

// Mask an address down to the enclosing oilpan heap base page.  All oilpan heap
// pages are aligned at blinkPageBase plus the size of a guard size.
// FIXME: Remove PLATFORM_EXPORT once we get a proper public interface to our
// typed arenas.  This is only exported to enable tests in HeapTest.cpp.
PLATFORM_EXPORT inline BasePage* pageFromObject(const void* object)
{
    Address address = reinterpret_cast<Address>(const_cast<void*>(object));
    BasePage* page = reinterpret_cast<BasePage*>(blinkPageAddress(address) + blinkGuardPageSize);
    ASSERT(page->contains(address));
    return page;
}

NO_SANITIZE_ADDRESS inline
size_t HeapObjectHeader::size() const
{
    size_t result = m_encoded & headerSizeMask;
    // Large objects should not refer to header->size().
    // The actual size of a large object is stored in
    // LargeObjectPage::m_payloadSize.
    ASSERT(result != largeObjectSizeInHeader);
    ASSERT(!pageFromObject(this)->isLargeObjectPage());
    return result;
}

#if ENABLE(ASSERT)
NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::checkHeader() const
{
    return !pageFromObject(this)->orphaned() && m_magic == magic;
}
#endif

inline Address HeapObjectHeader::payload()
{
    return reinterpret_cast<Address>(this) + sizeof(HeapObjectHeader);
}

inline Address HeapObjectHeader::payloadEnd()
{
    return reinterpret_cast<Address>(this) + size();
}

NO_SANITIZE_ADDRESS inline
size_t HeapObjectHeader::payloadSize()
{
    size_t size = m_encoded & headerSizeMask;
    if (UNLIKELY(size == largeObjectSizeInHeader)) {
        ASSERT(pageFromObject(this)->isLargeObjectPage());
        return static_cast<LargeObjectPage*>(pageFromObject(this))->payloadSize();
    }
    ASSERT(!pageFromObject(this)->isLargeObjectPage());
    return size - sizeof(HeapObjectHeader);
}

inline HeapObjectHeader* HeapObjectHeader::fromPayload(const void* payload)
{
    Address addr = reinterpret_cast<Address>(const_cast<void*>(payload));
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(addr - sizeof(HeapObjectHeader));
    ASSERT(header->checkHeader());
    return header;
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isWrapperHeaderMarked() const
{
    ASSERT(checkHeader());
    return m_encoded & headerWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::markWrapperHeader()
{
    ASSERT(checkHeader());
    ASSERT(!isWrapperHeaderMarked());
    m_encoded |= headerWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::unmarkWrapperHeader()
{
    ASSERT(checkHeader());
    ASSERT(isWrapperHeaderMarked());
    m_encoded &= ~headerWrapperMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isMarked() const
{
    ASSERT(checkHeader());
    return m_encoded & headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::mark()
{
    ASSERT(checkHeader());
    ASSERT(!isMarked());
    m_encoded = m_encoded | headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::unmark()
{
    ASSERT(checkHeader());
    ASSERT(isMarked());
    m_encoded &= ~headerMarkBitMask;
}

NO_SANITIZE_ADDRESS inline
bool HeapObjectHeader::isDead() const
{
    ASSERT(checkHeader());
    return m_encoded & headerDeadBitMask;
}

NO_SANITIZE_ADDRESS inline
void HeapObjectHeader::markDead()
{
    ASSERT(checkHeader());
    ASSERT(!isMarked());
    m_encoded |= headerDeadBitMask;
}

inline Address NormalPageArena::allocateObject(size_t allocationSize, size_t gcInfoIndex)
{
    if (LIKELY(allocationSize <= m_remainingAllocationSize)) {
        Address headerAddress = m_currentAllocationPoint;
        m_currentAllocationPoint += allocationSize;
        m_remainingAllocationSize -= allocationSize;
        ASSERT(gcInfoIndex > 0);
        new (NotNull, headerAddress) HeapObjectHeader(allocationSize, gcInfoIndex);
        Address result = headerAddress + sizeof(HeapObjectHeader);
        ASSERT(!(reinterpret_cast<uintptr_t>(result) & allocationMask));

        SET_MEMORY_ACCESSIBLE(result, allocationSize - sizeof(HeapObjectHeader));
        ASSERT(findPageFromAddress(headerAddress + allocationSize - 1));
        return result;
    }
    return outOfLineAllocate(allocationSize, gcInfoIndex);
}

inline NormalPageArena* NormalPage::arenaForNormalPage() const
{
    return static_cast<NormalPageArena*>(arena());
}

} // namespace blink

#endif // HeapPage_h
