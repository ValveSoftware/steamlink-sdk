// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/PageMemory.h"

#include "platform/heap/Heap.h"
#include "wtf/Assertions.h"
#include "wtf/Atomics.h"
#include "wtf/allocator/PageAllocator.h"

namespace blink {

void MemoryRegion::release()
{
    WTF::freePages(m_base, m_size);
}

bool MemoryRegion::commit()
{
    WTF::recommitSystemPages(m_base, m_size);
    return WTF::setSystemPagesAccessible(m_base, m_size);
}

void MemoryRegion::decommit()
{
    WTF::decommitSystemPages(m_base, m_size);
    WTF::setSystemPagesInaccessible(m_base, m_size);
}


PageMemoryRegion::PageMemoryRegion(Address base, size_t size, unsigned numPages, RegionTree* regionTree)
    : MemoryRegion(base, size)
    , m_isLargePage(numPages == 1)
    , m_numPages(numPages)
    , m_regionTree(regionTree)
{
    m_regionTree->add(this);
    for (size_t i = 0; i < blinkPagesPerRegion; ++i)
        m_inUse[i] = false;
}

PageMemoryRegion::~PageMemoryRegion()
{
    if (m_regionTree)
        m_regionTree->remove(this);
    release();
}

void PageMemoryRegion::pageDeleted(Address page)
{
    markPageUnused(page);
    if (!atomicDecrement(&m_numPages))
        delete this;
}

// TODO(haraken): Like partitionOutOfMemoryWithLotsOfUncommitedPages(),
// we should probably have a way to distinguish physical memory OOM from
// virtual address space OOM.
static NEVER_INLINE void blinkGCOutOfMemory()
{
    IMMEDIATE_CRASH();
}

PageMemoryRegion* PageMemoryRegion::allocate(size_t size, unsigned numPages, RegionTree* regionTree)
{
    // Round size up to the allocation granularity.
    size = (size + WTF::kPageAllocationGranularityOffsetMask) & WTF::kPageAllocationGranularityBaseMask;
    Address base = static_cast<Address>(WTF::allocPages(nullptr, size, blinkPageSize, WTF::PageInaccessible));
    if (!base)
        blinkGCOutOfMemory();
    return new PageMemoryRegion(base, size, numPages, regionTree);
}

PageMemoryRegion* RegionTree::lookup(Address address)
{
    MutexLocker locker(m_mutex);
    RegionTreeNode* current = m_root;
    while (current) {
        Address base = current->m_region->base();
        if (address < base) {
            current = current->m_left;
            continue;
        }
        if (address >= base + current->m_region->size()) {
            current = current->m_right;
            continue;
        }
        ASSERT(current->m_region->contains(address));
        return current->m_region;
    }
    return nullptr;
}

void RegionTree::add(PageMemoryRegion* region)
{
    ASSERT(region);
    RegionTreeNode* newTree = new RegionTreeNode(region);
    MutexLocker locker(m_mutex);
    newTree->addTo(&m_root);
}

void RegionTreeNode::addTo(RegionTreeNode** context)
{
    Address base = m_region->base();
    for (RegionTreeNode* current = *context; current; current = *context) {
        ASSERT(!current->m_region->contains(base));
        context = (base < current->m_region->base()) ? &current->m_left : &current->m_right;
    }
    *context = this;
}

void RegionTree::remove(PageMemoryRegion* region)
{
    // Deletion of large objects (and thus their regions) can happen
    // concurrently on sweeper threads.  Removal can also happen during thread
    // shutdown, but that case is safe.  Regardless, we make all removals
    // mutually exclusive.
    MutexLocker locker(m_mutex);
    ASSERT(region);
    ASSERT(m_root);
    Address base = region->base();
    RegionTreeNode** context = &m_root;
    RegionTreeNode* current = m_root;
    for (; current; current = *context) {
        if (region == current->m_region)
            break;
        context = (base < current->m_region->base()) ? &current->m_left : &current->m_right;
    }

    // Shutdown via detachMainThread might not have populated the region tree.
    if (!current)
        return;

    *context = nullptr;
    if (current->m_left) {
        current->m_left->addTo(context);
        current->m_left = nullptr;
    }
    if (current->m_right) {
        current->m_right->addTo(context);
        current->m_right = nullptr;
    }
    delete current;
}

PageMemory::PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable)
    : m_reserved(reserved)
    , m_writable(writable)
{
    ASSERT(reserved->contains(writable));

    // Register the writable area of the memory as part of the LSan root set.
    // Only the writable area is mapped and can contain C++ objects.  Those
    // C++ objects can contain pointers to objects outside of the heap and
    // should therefore be part of the LSan root set.
    __lsan_register_root_region(m_writable.base(), m_writable.size());
}

PageMemory* PageMemory::setupPageMemoryInRegion(PageMemoryRegion* region, size_t pageOffset, size_t payloadSize)
{
    // Setup the payload one guard page into the page memory.
    Address payloadAddress = region->base() + pageOffset + blinkGuardPageSize;
    return new PageMemory(region, MemoryRegion(payloadAddress, payloadSize));
}

static size_t roundToOsPageSize(size_t size)
{
    return (size + WTF::kSystemPageSize - 1) & ~(WTF::kSystemPageSize - 1);
}

PageMemory* PageMemory::allocate(size_t payloadSize, RegionTree* regionTree)
{
    ASSERT(payloadSize > 0);

    // Virtual memory allocation routines operate in OS page sizes.
    // Round up the requested size to nearest os page size.
    payloadSize = roundToOsPageSize(payloadSize);

    // Overallocate by 2 times OS page size to have space for a
    // guard page at the beginning and end of blink heap page.
    size_t allocationSize = payloadSize + 2 * blinkGuardPageSize;
    PageMemoryRegion* pageMemoryRegion = PageMemoryRegion::allocateLargePage(allocationSize, regionTree);
    PageMemory* storage = setupPageMemoryInRegion(pageMemoryRegion, 0, payloadSize);
    RELEASE_ASSERT(storage->commit());
    return storage;
}

} // namespace blink
