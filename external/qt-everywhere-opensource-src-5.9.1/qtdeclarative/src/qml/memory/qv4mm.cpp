/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4engine_p.h"
#include "qv4object_p.h"
#include "qv4objectproto_p.h"
#include "qv4mm_p.h"
#include "qv4qobjectwrapper_p.h"
#include <QtCore/qalgorithms.h>
#include <QtCore/private/qnumeric_p.h>
#include <qqmlengine.h>
#include "PageReservation.h"
#include "PageAllocation.h"
#include "PageAllocationAligned.h"
#include "StdLibExtras.h"

#include <QElapsedTimer>
#include <QMap>
#include <QScopedValueRollback>

#include <iostream>
#include <cstdlib>
#include <algorithm>
#include "qv4alloca_p.h"
#include "qv4profiling_p.h"

#define MM_DEBUG 0

#if MM_DEBUG
#define DEBUG qDebug() << "MM:"
#else
#define DEBUG if (1) ; else qDebug() << "MM:"
#endif

#ifdef V4_USE_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#endif

#ifdef V4_USE_HEAPTRACK
#include <heaptrack_api.h>
#endif

#if OS(QNX)
#include <sys/storage.h>   // __tls()
#endif

#if USE(PTHREADS) && HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#define MIN_UNMANAGED_HEAPSIZE_GC_LIMIT std::size_t(128 * 1024)

using namespace WTF;

QT_BEGIN_NAMESPACE

namespace QV4 {

enum {
    MinSlotsGCLimit = QV4::Chunk::AvailableSlots*16,
    GCOverallocation = 200 /* Max overallocation by the GC in % */
};

struct MemorySegment {
    enum {
        NumChunks = 8*sizeof(quint64),
        SegmentSize = NumChunks*Chunk::ChunkSize,
    };

    MemorySegment(size_t size)
    {
        size += Chunk::ChunkSize; // make sure we can get enough 64k aligment memory
        if (size < SegmentSize)
            size = SegmentSize;

        pageReservation = PageReservation::reserve(size, OSAllocator::JSGCHeapPages);
        base = reinterpret_cast<Chunk *>((reinterpret_cast<quintptr>(pageReservation.base()) + Chunk::ChunkSize - 1) & ~(Chunk::ChunkSize - 1));
        nChunks = NumChunks;
        availableBytes = size - (reinterpret_cast<quintptr>(base) - reinterpret_cast<quintptr>(pageReservation.base()));
        if (availableBytes < SegmentSize)
            --nChunks;
    }
    MemorySegment(MemorySegment &&other) {
        qSwap(pageReservation, other.pageReservation);
        qSwap(base, other.base);
        qSwap(allocatedMap, other.allocatedMap);
        qSwap(availableBytes, other.availableBytes);
        qSwap(nChunks, other.nChunks);
    }

    ~MemorySegment() {
        if (base)
            pageReservation.deallocate();
    }

    void setBit(size_t index) {
        Q_ASSERT(index < nChunks);
        quint64 bit = static_cast<quint64>(1) << index;
//        qDebug() << "    setBit" << hex << index << (index & (Bits - 1)) << bit;
        allocatedMap |= bit;
    }
    void clearBit(size_t index) {
        Q_ASSERT(index < nChunks);
        quint64 bit = static_cast<quint64>(1) << index;
//        qDebug() << "    setBit" << hex << index << (index & (Bits - 1)) << bit;
        allocatedMap &= ~bit;
    }
    bool testBit(size_t index) const {
        Q_ASSERT(index < nChunks);
        quint64 bit = static_cast<quint64>(1) << index;
        return (allocatedMap & bit);
    }

    Chunk *allocate(size_t size);
    void free(Chunk *chunk, size_t size) {
        DEBUG << "freeing chunk" << chunk;
        size_t index = static_cast<size_t>(chunk - base);
        size_t end = qMin(static_cast<size_t>(NumChunks), index + (size - 1)/Chunk::ChunkSize + 1);
        while (index < end) {
            Q_ASSERT(testBit(index));
            clearBit(index);
            ++index;
        }

        size_t pageSize = WTF::pageSize();
        size = (size + pageSize - 1) & ~(pageSize - 1);
#if !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)
        // Linux and Windows zero out pages that have been decommitted and get committed again.
        // unfortunately that's not true on other OSes (e.g. BSD based ones), so zero out the
        // memory before decommit, so that we can be sure that all chunks we allocate will be
        // zero initialized.
        memset(chunk, 0, size);
#endif
        pageReservation.decommit(chunk, size);
    }

    bool contains(Chunk *c) const {
        return c >= base && c < base + nChunks;
    }

    PageReservation pageReservation;
    Chunk *base = 0;
    quint64 allocatedMap = 0;
    size_t availableBytes = 0;
    uint nChunks = 0;
};

Chunk *MemorySegment::allocate(size_t size)
{
    if (!allocatedMap && size >= SegmentSize) {
        // chunk allocated for one huge allocation
        Q_ASSERT(availableBytes >= size);
        pageReservation.commit(base, size);
        allocatedMap = ~static_cast<quintptr>(0);
        return base;
    }
    size_t requiredChunks = (size + sizeof(Chunk) - 1)/sizeof(Chunk);
    uint sequence = 0;
    Chunk *candidate = 0;
    for (uint i = 0; i < nChunks; ++i) {
        if (!testBit(i)) {
            if (!candidate)
                candidate = base + i;
            ++sequence;
        } else {
            candidate = 0;
            sequence = 0;
        }
        if (sequence == requiredChunks) {
            pageReservation.commit(candidate, size);
            for (uint i = 0; i < requiredChunks; ++i)
                setBit(candidate - base + i);
            DEBUG << "allocated chunk " << candidate << hex << size;
            return candidate;
        }
    }
    return 0;
}

struct ChunkAllocator {
    ChunkAllocator() {}

    size_t requiredChunkSize(size_t size) {
        size += Chunk::HeaderSize; // space required for the Chunk header
        size_t pageSize = WTF::pageSize();
        size = (size + pageSize - 1) & ~(pageSize - 1); // align to page sizes
        if (size < Chunk::ChunkSize)
            size = Chunk::ChunkSize;
        return size;
    }

    Chunk *allocate(size_t size = 0);
    void free(Chunk *chunk, size_t size = 0);

    std::vector<MemorySegment> memorySegments;
};

Chunk *ChunkAllocator::allocate(size_t size)
{
    size = requiredChunkSize(size);
    for (auto &m : memorySegments) {
        if (~m.allocatedMap) {
            Chunk *c = m.allocate(size);
            if (c)
                return c;
        }
    }

    // allocate a new segment
    memorySegments.push_back(MemorySegment(size));
    Chunk *c = memorySegments.back().allocate(size);
    Q_ASSERT(c);
    return c;
}

void ChunkAllocator::free(Chunk *chunk, size_t size)
{
    size = requiredChunkSize(size);
    for (auto &m : memorySegments) {
        if (m.contains(chunk)) {
            m.free(chunk, size);
            return;
        }
    }
    Q_ASSERT(false);
}

#ifdef DUMP_SWEEP
QString binary(quintptr n) {
    QString s = QString::number(n, 2);
    while (s.length() < 64)
        s.prepend(QChar::fromLatin1('0'));
    return s;
}
#define SDUMP qDebug
#else
QString binary(quintptr) { return QString(); }
#define SDUMP if (1) ; else qDebug
#endif

bool Chunk::sweep()
{
    bool hasUsedSlots = false;
    SDUMP() << "sweeping chunk" << this;
    HeapItem *o = realBase();
    bool lastSlotFree = false;
    for (uint i = 0; i < Chunk::EntriesInBitmap; ++i) {
        Q_ASSERT((grayBitmap[i] | blackBitmap[i]) == blackBitmap[i]); // check that we don't have gray only objects
        quintptr toFree = objectBitmap[i] ^ blackBitmap[i];
        Q_ASSERT((toFree & objectBitmap[i]) == toFree); // check all black objects are marked as being used
        quintptr e = extendsBitmap[i];
        SDUMP() << "   index=" << i;
        SDUMP() << "        toFree      =" << binary(toFree);
        SDUMP() << "        black       =" << binary(blackBitmap[i]);
        SDUMP() << "        object      =" << binary(objectBitmap[i]);
        SDUMP() << "        extends     =" << binary(e);
        if (lastSlotFree)
            e &= (e + 1); // clear all lowest extent bits
        while (toFree) {
            uint index = qCountTrailingZeroBits(toFree);
            quintptr bit = (static_cast<quintptr>(1) << index);

            toFree ^= bit; // mask out freed slot
            //            DEBUG << "       index" << hex << index << toFree;

            // remove all extends slots that have been freed
            // this is a bit of bit trickery.
            quintptr mask = (bit << 1) - 1; // create a mask of 1's to the right of and up to the current bit
            quintptr objmask = e | mask; // or'ing mask with e gives all ones until the end of the current object
            quintptr result = objmask + 1;
            Q_ASSERT(qCountTrailingZeroBits(result) - index != 0); // ensure we freed something
            result |= mask; // ensure we don't clear stuff to the right of the current object
            e &= result;

            HeapItem *itemToFree = o + index;
            Heap::Base *b = *itemToFree;
            if (b->vtable()->destroy) {
                b->vtable()->destroy(b);
                b->_checkIsDestroyed();
            }
        }
        objectBitmap[i] = blackBitmap[i];
        hasUsedSlots |= (blackBitmap[i] != 0);
        blackBitmap[i] = 0;
        extendsBitmap[i] = e;
        lastSlotFree = !((objectBitmap[i]|extendsBitmap[i]) >> (sizeof(quintptr)*8 - 1));
        SDUMP() << "        new extends =" << binary(e);
        SDUMP() << "        lastSlotFree" << lastSlotFree;
        Q_ASSERT((objectBitmap[i] & extendsBitmap[i]) == 0);
        o += Chunk::Bits;
    }
    //    DEBUG << "swept chunk" << this << "freed" << slotsFreed << "slots.";
    return hasUsedSlots;
}

void Chunk::freeAll()
{
    //    DEBUG << "sweeping chunk" << this << (*freeList);
    HeapItem *o = realBase();
    for (uint i = 0; i < Chunk::EntriesInBitmap; ++i) {
        quintptr toFree = objectBitmap[i];
        quintptr e = extendsBitmap[i];
        //        DEBUG << hex << "   index=" << i << toFree;
        while (toFree) {
            uint index = qCountTrailingZeroBits(toFree);
            quintptr bit = (static_cast<quintptr>(1) << index);

            toFree ^= bit; // mask out freed slot
            //            DEBUG << "       index" << hex << index << toFree;

            // remove all extends slots that have been freed
            // this is a bit of bit trickery.
            quintptr mask = (bit << 1) - 1; // create a mask of 1's to the right of and up to the current bit
            quintptr objmask = e | mask; // or'ing mask with e gives all ones until the end of the current object
            quintptr result = objmask + 1;
            Q_ASSERT(qCountTrailingZeroBits(result) - index != 0); // ensure we freed something
            result |= mask; // ensure we don't clear stuff to the right of the current object
            e &= result;

            HeapItem *itemToFree = o + index;
            Heap::Base *b = *itemToFree;
            if (b->vtable()->destroy) {
                b->vtable()->destroy(b);
                b->_checkIsDestroyed();
            }
        }
        objectBitmap[i] = 0;
        blackBitmap[i] = 0;
        extendsBitmap[i] = e;
        o += Chunk::Bits;
    }
    //    DEBUG << "swept chunk" << this << "freed" << slotsFreed << "slots.";
}

void Chunk::sortIntoBins(HeapItem **bins, uint nBins)
{
//    qDebug() << "sortIntoBins:";
    HeapItem *base = realBase();
#if QT_POINTER_SIZE == 8
    const int start = 0;
#else
    const int start = 1;
#endif
#ifndef QT_NO_DEBUG
    uint freeSlots = 0;
    uint allocatedSlots = 0;
#endif
    for (int i = start; i < EntriesInBitmap; ++i) {
        quintptr usedSlots = (objectBitmap[i]|extendsBitmap[i]);
#if QT_POINTER_SIZE == 8
        if (!i)
            usedSlots |= (static_cast<quintptr>(1) << (HeaderSize/SlotSize)) - 1;
#endif
#ifndef QT_NO_DEBUG
        allocatedSlots += qPopulationCount(usedSlots);
//        qDebug() << hex << "   i=" << i << "used=" << usedSlots;
#endif
        while (1) {
            uint index = qCountTrailingZeroBits(usedSlots + 1);
            if (index == Bits)
                break;
            uint freeStart = i*Bits + index;
            usedSlots &= ~((static_cast<quintptr>(1) << index) - 1);
            while (!usedSlots) {
                ++i;
                if (i == EntriesInBitmap) {
                    usedSlots = (quintptr)-1;
                    break;
                }
                usedSlots = (objectBitmap[i]|extendsBitmap[i]);
#ifndef QT_NO_DEBUG
                allocatedSlots += qPopulationCount(usedSlots);
//                qDebug() << hex << "   i=" << i << "used=" << usedSlots;
#endif
            }
            HeapItem *freeItem = base + freeStart;

            index = qCountTrailingZeroBits(usedSlots);
            usedSlots |= (quintptr(1) << index) - 1;
            uint freeEnd = i*Bits + index;
            uint nSlots = freeEnd - freeStart;
#ifndef QT_NO_DEBUG
//            qDebug() << hex << "   got free slots from" << freeStart << "to" << freeEnd << "n=" << nSlots << "usedSlots=" << usedSlots;
            freeSlots += nSlots;
#endif
            Q_ASSERT(freeEnd > freeStart && freeEnd <= NumSlots);
            freeItem->freeData.availableSlots = nSlots;
            uint bin = qMin(nBins - 1, nSlots);
            freeItem->freeData.next = bins[bin];
            bins[bin] = freeItem;
        }
    }
#ifndef QT_NO_DEBUG
    Q_ASSERT(freeSlots + allocatedSlots == (EntriesInBitmap - start) * 8 * sizeof(quintptr));
#endif
}


template<typename T>
StackAllocator<T>::StackAllocator(ChunkAllocator *chunkAlloc)
    : chunkAllocator(chunkAlloc)
{
    chunks.push_back(chunkAllocator->allocate());
    firstInChunk = chunks.back()->first();
    nextFree = firstInChunk;
    lastInChunk = firstInChunk + (Chunk::AvailableSlots - 1)/requiredSlots*requiredSlots;
}

template<typename T>
void StackAllocator<T>::freeAll()
{
    for (auto c : chunks)
        chunkAllocator->free(c);
}

template<typename T>
void StackAllocator<T>::nextChunk() {
    Q_ASSERT(nextFree == lastInChunk);
    ++currentChunk;
    if (currentChunk >= chunks.size()) {
        Chunk *newChunk = chunkAllocator->allocate();
        chunks.push_back(newChunk);
    }
    firstInChunk = chunks.at(currentChunk)->first();
    nextFree = firstInChunk;
    lastInChunk = firstInChunk + (Chunk::AvailableSlots - 1)/requiredSlots*requiredSlots;
}

template<typename T>
void QV4::StackAllocator<T>::prevChunk() {
    Q_ASSERT(nextFree == firstInChunk);
    Q_ASSERT(chunks.at(currentChunk) == nextFree->chunk());
    Q_ASSERT(currentChunk > 0);
    --currentChunk;
    firstInChunk = chunks.at(currentChunk)->first();
    lastInChunk = firstInChunk + (Chunk::AvailableSlots - 1)/requiredSlots*requiredSlots;
    nextFree = lastInChunk;
}

template struct StackAllocator<Heap::CallContext>;


HeapItem *BlockAllocator::allocate(size_t size, bool forceAllocation) {
    Q_ASSERT((size % Chunk::SlotSize) == 0);
    size_t slotsRequired = size >> Chunk::SlotSizeShift;
#if MM_DEBUG
    ++allocations[bin];
#endif

    HeapItem **last;

    HeapItem *m;

    if (slotsRequired < NumBins - 1) {
        m = freeBins[slotsRequired];
        if (m) {
            freeBins[slotsRequired] = m->freeData.next;
            goto done;
        }
    }

    if (nFree >= slotsRequired) {
        // use bump allocation
        Q_ASSERT(nextFree);
        m = nextFree;
        nextFree += slotsRequired;
        nFree -= slotsRequired;
        goto done;
    }

    //        DEBUG << "No matching bin found for item" << size << bin;
    // search last bin for a large enough item
    last = &freeBins[NumBins - 1];
    while ((m = *last)) {
        if (m->freeData.availableSlots >= slotsRequired) {
            *last = m->freeData.next; // take it out of the list

            size_t remainingSlots = m->freeData.availableSlots - slotsRequired;
            //                DEBUG << "found large free slots of size" << m->freeData.availableSlots << m << "remaining" << remainingSlots;
            if (remainingSlots == 0)
                goto done;

            HeapItem *remainder = m + slotsRequired;
            if (remainingSlots > nFree) {
                if (nFree) {
                    size_t bin = binForSlots(nFree);
                    nextFree->freeData.next = freeBins[bin];
                    nextFree->freeData.availableSlots = nFree;
                    freeBins[bin] = nextFree;
                }
                nextFree = remainder;
                nFree = remainingSlots;
            } else {
                remainder->freeData.availableSlots = remainingSlots;
                size_t binForRemainder = binForSlots(remainingSlots);
                remainder->freeData.next = freeBins[binForRemainder];
                freeBins[binForRemainder] = remainder;
            }
            goto done;
        }
        last = &m->freeData.next;
    }

    if (slotsRequired < NumBins - 1) {
        // check if we can split up another slot
        for (size_t i = slotsRequired + 1; i < NumBins - 1; ++i) {
            m = freeBins[i];
            if (m) {
                freeBins[i] = m->freeData.next; // take it out of the list
//                qDebug() << "got item" << slotsRequired << "from slot" << i;
                size_t remainingSlots = i - slotsRequired;
                Q_ASSERT(remainingSlots < NumBins - 1);
                HeapItem *remainder = m + slotsRequired;
                remainder->freeData.availableSlots = remainingSlots;
                remainder->freeData.next = freeBins[remainingSlots];
                freeBins[remainingSlots] = remainder;
                goto done;
            }
        }
    }

    if (!m) {
        if (!forceAllocation)
            return 0;
        Chunk *newChunk = chunkAllocator->allocate();
        chunks.push_back(newChunk);
        nextFree = newChunk->first();
        nFree = Chunk::AvailableSlots;
        m = nextFree;
        nextFree += slotsRequired;
        nFree -= slotsRequired;
    }

done:
    m->setAllocatedSlots(slotsRequired);
    //        DEBUG << "   " << hex << m->chunk() << m->chunk()->objectBitmap[0] << m->chunk()->extendsBitmap[0] << (m - m->chunk()->realBase());
    return m;
}

void BlockAllocator::sweep()
{
    nextFree = 0;
    nFree = 0;
    memset(freeBins, 0, sizeof(freeBins));

//    qDebug() << "BlockAlloc: sweep";
    usedSlotsAfterLastSweep = 0;

    auto isFree = [this] (Chunk *c) {
        bool isUsed = c->sweep();

        if (isUsed) {
            c->sortIntoBins(freeBins, NumBins);
            usedSlotsAfterLastSweep += c->nUsedSlots();
        } else {
            chunkAllocator->free(c);
        }
        return !isUsed;
    };

    auto newEnd = std::remove_if(chunks.begin(), chunks.end(), isFree);
    chunks.erase(newEnd, chunks.end());
}

void BlockAllocator::freeAll()
{
    for (auto c : chunks) {
        c->freeAll();
        chunkAllocator->free(c);
    }
}

#if MM_DEBUG
void BlockAllocator::stats() {
    DEBUG << "MM stats:";
    QString s;
    for (int i = 0; i < 10; ++i) {
        uint c = 0;
        HeapItem *item = freeBins[i];
        while (item) {
            ++c;
            item = item->freeData.next;
        }
        s += QString::number(c) + QLatin1String(", ");
    }
    HeapItem *item = freeBins[NumBins - 1];
    uint c = 0;
    while (item) {
        ++c;
        item = item->freeData.next;
    }
    s += QLatin1String("..., ") + QString::number(c);
    DEBUG << "bins:" << s;
    QString a;
    for (int i = 0; i < 10; ++i)
        a += QString::number(allocations[i]) + QLatin1String(", ");
    a += QLatin1String("..., ") + QString::number(allocations[NumBins - 1]);
    DEBUG << "allocs:" << a;
    memset(allocations, 0, sizeof(allocations));
}
#endif


HeapItem *HugeItemAllocator::allocate(size_t size) {
    Chunk *c = chunkAllocator->allocate(size);
    chunks.push_back(HugeChunk{c, size});
    Chunk::setBit(c->objectBitmap, c->first() - c->realBase());
    return c->first();
}

static void freeHugeChunk(ChunkAllocator *chunkAllocator, const HugeItemAllocator::HugeChunk &c)
{
    HeapItem *itemToFree = c.chunk->first();
    Heap::Base *b = *itemToFree;
    if (b->vtable()->destroy) {
        b->vtable()->destroy(b);
        b->_checkIsDestroyed();
    }
    chunkAllocator->free(c.chunk, c.size);
}

void HugeItemAllocator::sweep() {
    auto isBlack = [this] (const HugeChunk &c) {
        bool b = c.chunk->first()->isBlack();
        Chunk::clearBit(c.chunk->blackBitmap, c.chunk->first() - c.chunk->realBase());
        if (!b)
            freeHugeChunk(chunkAllocator, c);
        return !b;
    };

    auto newEnd = std::remove_if(chunks.begin(), chunks.end(), isBlack);
    chunks.erase(newEnd, chunks.end());
}

void HugeItemAllocator::freeAll()
{
    for (auto &c : chunks) {
        freeHugeChunk(chunkAllocator, c);
    }
}


MemoryManager::MemoryManager(ExecutionEngine *engine)
    : engine(engine)
    , chunkAllocator(new ChunkAllocator)
    , stackAllocator(chunkAllocator)
    , blockAllocator(chunkAllocator)
    , hugeItemAllocator(chunkAllocator)
    , m_persistentValues(new PersistentValueStorage(engine))
    , m_weakValues(new PersistentValueStorage(engine))
    , unmanagedHeapSizeGCLimit(MIN_UNMANAGED_HEAPSIZE_GC_LIMIT)
    , aggressiveGC(!qEnvironmentVariableIsEmpty("QV4_MM_AGGRESSIVE_GC"))
    , gcStats(!qEnvironmentVariableIsEmpty(QV4_MM_STATS))
{
#ifdef V4_USE_VALGRIND
    VALGRIND_CREATE_MEMPOOL(this, 0, true);
#endif
}

#ifndef QT_NO_DEBUG
static size_t lastAllocRequestedSlots = 0;
#endif

Heap::Base *MemoryManager::allocString(std::size_t unmanagedSize)
{
    const size_t stringSize = align(sizeof(Heap::String));
#ifndef QT_NO_DEBUG
    lastAllocRequestedSlots = stringSize >> Chunk::SlotSizeShift;
#endif

    bool didGCRun = false;
    if (aggressiveGC) {
        runGC();
        didGCRun = true;
    }

    unmanagedHeapSize += unmanagedSize;
    if (unmanagedHeapSize > unmanagedHeapSizeGCLimit) {
        runGC();

        if (3*unmanagedHeapSizeGCLimit <= 4*unmanagedHeapSize)
            // more than 75% full, raise limit
            unmanagedHeapSizeGCLimit = std::max(unmanagedHeapSizeGCLimit, unmanagedHeapSize) * 2;
        else if (unmanagedHeapSize * 4 <= unmanagedHeapSizeGCLimit)
            // less than 25% full, lower limit
            unmanagedHeapSizeGCLimit = qMax(MIN_UNMANAGED_HEAPSIZE_GC_LIMIT, unmanagedHeapSizeGCLimit/2);
        didGCRun = true;
    }

    HeapItem *m = blockAllocator.allocate(stringSize);
    if (!m) {
        if (!didGCRun && shouldRunGC())
            runGC();
        m = blockAllocator.allocate(stringSize, true);
    }

    memset(m, 0, stringSize);
    return *m;
}

Heap::Base *MemoryManager::allocData(std::size_t size)
{
#ifndef QT_NO_DEBUG
    lastAllocRequestedSlots = size >> Chunk::SlotSizeShift;
#endif

    bool didRunGC = false;
    if (aggressiveGC) {
        runGC();
        didRunGC = true;
    }
#ifdef DETAILED_MM_STATS
    willAllocate(size);
#endif // DETAILED_MM_STATS

    Q_ASSERT(size >= Chunk::SlotSize);
    Q_ASSERT(size % Chunk::SlotSize == 0);

//    qDebug() << "unmanagedHeapSize:" << unmanagedHeapSize << "limit:" << unmanagedHeapSizeGCLimit << "unmanagedSize:" << unmanagedSize;

    if (size > Chunk::DataSize)
        return *hugeItemAllocator.allocate(size);

    HeapItem *m = blockAllocator.allocate(size);
    if (!m) {
        if (!didRunGC && shouldRunGC())
            runGC();
        m = blockAllocator.allocate(size, true);
    }

    memset(m, 0, size);
    return *m;
}

Heap::Object *MemoryManager::allocObjectWithMemberData(const QV4::VTable *vtable, uint nMembers)
{
    uint size = (vtable->nInlineProperties + vtable->inlinePropertyOffset)*sizeof(Value);
    Q_ASSERT(!(size % sizeof(HeapItem)));
    Heap::Object *o = static_cast<Heap::Object *>(allocData(size));

    // ### Could optimize this and allocate both in one go through the block allocator
    if (nMembers > vtable->nInlineProperties) {
        nMembers -= vtable->nInlineProperties;
        std::size_t memberSize = align(sizeof(Heap::MemberData) + (nMembers - 1)*sizeof(Value));
//        qDebug() << "allocating member data for" << o << nMembers << memberSize;
        Heap::Base *m;
        if (memberSize > Chunk::DataSize)
            m = *hugeItemAllocator.allocate(memberSize);
        else
            m = *blockAllocator.allocate(memberSize, true);
        memset(m, 0, memberSize);
        o->memberData = static_cast<Heap::MemberData *>(m);
        o->memberData->internalClass = engine->internalClasses[EngineBase::Class_MemberData];
        Q_ASSERT(o->memberData->internalClass);
        o->memberData->size = static_cast<uint>((memberSize - sizeof(Heap::MemberData) + sizeof(Value))/sizeof(Value));
        o->memberData->init();
//        qDebug() << "    got" << o->memberData << o->memberData->size;
    }
    return o;
}

static void drainMarkStack(QV4::ExecutionEngine *engine, Value *markBase)
{
    while (engine->jsStackTop > markBase) {
        Heap::Base *h = engine->popForGC();
        Q_ASSERT(h); // at this point we should only have Heap::Base objects in this area on the stack. If not, weird things might happen.
        Q_ASSERT (h->vtable()->markObjects);
        h->vtable()->markObjects(h, engine);
    }
}

void MemoryManager::mark()
{
    Value *markBase = engine->jsStackTop;

    engine->markObjects();

    collectFromJSStack();

    m_persistentValues->mark(engine);

    // Preserve QObject ownership rules within JavaScript: A parent with c++ ownership
    // keeps all of its children alive in JavaScript.

    // Do this _after_ collectFromStack to ensure that processing the weak
    // managed objects in the loop down there doesn't make then end up as leftovers
    // on the stack and thus always get collected.
    for (PersistentValueStorage::Iterator it = m_weakValues->begin(); it != m_weakValues->end(); ++it) {
        QObjectWrapper *qobjectWrapper = (*it).as<QObjectWrapper>();
        if (!qobjectWrapper)
            continue;
        QObject *qobject = qobjectWrapper->object();
        if (!qobject)
            continue;
        bool keepAlive = QQmlData::keepAliveDuringGarbageCollection(qobject);

        if (!keepAlive) {
            if (QObject *parent = qobject->parent()) {
                while (parent->parent())
                    parent = parent->parent();

                keepAlive = QQmlData::keepAliveDuringGarbageCollection(parent);
            }
        }

        if (keepAlive)
            qobjectWrapper->mark(engine);

        if (engine->jsStackTop >= engine->jsStackLimit)
            drainMarkStack(engine, markBase);
    }

    drainMarkStack(engine, markBase);
}

void MemoryManager::sweep(bool lastSweep)
{
    for (PersistentValueStorage::Iterator it = m_weakValues->begin(); it != m_weakValues->end(); ++it) {
        Managed *m = (*it).managed();
        if (!m || m->markBit())
            continue;
        // we need to call destroyObject on qobjectwrappers now, so that they can emit the destroyed
        // signal before we start sweeping the heap
        if (QObjectWrapper *qobjectWrapper = (*it).as<QObjectWrapper>())
            qobjectWrapper->destroyObject(lastSweep);

        (*it) = Primitive::undefinedValue();
    }

    // onDestruction handlers may have accessed other QObject wrappers and reset their value, so ensure
    // that they are all set to undefined.
    for (PersistentValueStorage::Iterator it = m_weakValues->begin(); it != m_weakValues->end(); ++it) {
        Managed *m = (*it).managed();
        if (!m || m->markBit())
            continue;
        (*it) = Primitive::undefinedValue();
    }

    // Now it is time to free QV4::QObjectWrapper Value, we must check the Value's tag to make sure its object has been destroyed
    const int pendingCount = m_pendingFreedObjectWrapperValue.count();
    if (pendingCount) {
        QVector<Value *> remainingWeakQObjectWrappers;
        remainingWeakQObjectWrappers.reserve(pendingCount);
        for (int i = 0; i < pendingCount; ++i) {
            Value *v = m_pendingFreedObjectWrapperValue.at(i);
            if (v->isUndefined() || v->isEmpty())
                PersistentValueStorage::free(v);
            else
                remainingWeakQObjectWrappers.append(v);
        }
        m_pendingFreedObjectWrapperValue = remainingWeakQObjectWrappers;
    }

    if (MultiplyWrappedQObjectMap *multiplyWrappedQObjects = engine->m_multiplyWrappedQObjects) {
        for (MultiplyWrappedQObjectMap::Iterator it = multiplyWrappedQObjects->begin(); it != multiplyWrappedQObjects->end();) {
            if (!it.value().isNullOrUndefined())
                it = multiplyWrappedQObjects->erase(it);
            else
                ++it;
        }
    }

    blockAllocator.sweep();
    hugeItemAllocator.sweep();
}

bool MemoryManager::shouldRunGC() const
{
    size_t total = blockAllocator.totalSlots();
    size_t usedSlots = blockAllocator.usedSlotsAfterLastSweep;
    if (total > MinSlotsGCLimit && usedSlots * GCOverallocation < total * 100)
        return true;
    return false;
}

size_t dumpBins(BlockAllocator *b, bool printOutput = true)
{
    size_t totalFragmentedSlots = 0;
    if (printOutput)
        qDebug() << "Fragmentation map:";
    for (uint i = 0; i < BlockAllocator::NumBins; ++i) {
        uint nEntries = 0;
        HeapItem *h = b->freeBins[i];
        while (h) {
            ++nEntries;
            totalFragmentedSlots += h->freeData.availableSlots;
            h = h->freeData.next;
        }
        if (printOutput)
            qDebug() << "    number of entries in slot" << i << ":" << nEntries;
    }
    SDUMP() << "    large slot map";
    HeapItem *h = b->freeBins[BlockAllocator::NumBins - 1];
    while (h) {
        SDUMP() << "        " << hex << (quintptr(h)/32) << h->freeData.availableSlots;
        h = h->freeData.next;
    }

    if (printOutput)
        qDebug() << "  total mem in bins" << totalFragmentedSlots*Chunk::SlotSize;
    return totalFragmentedSlots*Chunk::SlotSize;
}

void MemoryManager::runGC()
{
    if (gcBlocked) {
//        qDebug() << "Not running GC.";
        return;
    }

    QScopedValueRollback<bool> gcBlocker(gcBlocked, true);

    if (!gcStats) {
//        uint oldUsed = allocator.usedMem();
        mark();
        sweep();
//        DEBUG << "RUN GC: allocated:" << allocator.allocatedMem() << "used before" << oldUsed << "used now" << allocator.usedMem();
    } else {
        bool triggeredByUnmanagedHeap = (unmanagedHeapSize > unmanagedHeapSizeGCLimit);
        size_t oldUnmanagedSize = unmanagedHeapSize;
        const size_t totalMem = getAllocatedMem();
        const size_t usedBefore = getUsedMem();
        const size_t largeItemsBefore = getLargeItemsMem();

        qDebug() << "========== GC ==========";
#ifndef QT_NO_DEBUG
        qDebug() << "    Triggered by alloc request of" << lastAllocRequestedSlots << "slots.";
#endif
        size_t oldChunks = blockAllocator.chunks.size();
        qDebug() << "Allocated" << totalMem << "bytes in" << oldChunks << "chunks";
        qDebug() << "Fragmented memory before GC" << (totalMem - usedBefore);
        dumpBins(&blockAllocator);

        QElapsedTimer t;
        t.start();
        mark();
        qint64 markTime = t.restart();
        sweep();
        const size_t usedAfter = getUsedMem();
        const size_t largeItemsAfter = getLargeItemsMem();
        qint64 sweepTime = t.elapsed();

        if (triggeredByUnmanagedHeap) {
            qDebug() << "triggered by unmanaged heap:";
            qDebug() << "   old unmanaged heap size:" << oldUnmanagedSize;
            qDebug() << "   new unmanaged heap:" << unmanagedHeapSize;
            qDebug() << "   unmanaged heap limit:" << unmanagedHeapSizeGCLimit;
        }
        size_t memInBins = dumpBins(&blockAllocator);
        qDebug() << "Marked object in" << markTime << "ms.";
        qDebug() << "Sweeped object in" << sweepTime << "ms.";
        qDebug() << "Used memory before GC:" << usedBefore;
        qDebug() << "Used memory after GC:" << usedAfter;
        qDebug() << "Freed up bytes:" << (usedBefore - usedAfter);
        qDebug() << "Freed up chunks:" << (oldChunks - blockAllocator.chunks.size());
        size_t lost = blockAllocator.allocatedMem() - memInBins - usedAfter;
        if (lost)
            qDebug() << "!!!!!!!!!!!!!!!!!!!!! LOST MEM:" << lost << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
        if (largeItemsBefore || largeItemsAfter) {
            qDebug() << "Large item memory before GC:" << largeItemsBefore;
            qDebug() << "Large item memory after GC:" << largeItemsAfter;
            qDebug() << "Large item memory freed up:" << (largeItemsBefore - largeItemsAfter);
        }
        qDebug() << "======== End GC ========";
    }

    if (aggressiveGC) {
        // ensure we don't 'loose' any memory
        Q_ASSERT(blockAllocator.allocatedMem() == getUsedMem() + dumpBins(&blockAllocator, false));
    }
}

size_t MemoryManager::getUsedMem() const
{
    return blockAllocator.usedMem();
}

size_t MemoryManager::getAllocatedMem() const
{
    return blockAllocator.allocatedMem() + hugeItemAllocator.usedMem();
}

size_t MemoryManager::getLargeItemsMem() const
{
    return hugeItemAllocator.usedMem();
}

MemoryManager::~MemoryManager()
{
    delete m_persistentValues;

    sweep(/*lastSweep*/true);
    blockAllocator.freeAll();
    hugeItemAllocator.freeAll();
    stackAllocator.freeAll();

    delete m_weakValues;
#ifdef V4_USE_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(this);
#endif
    delete chunkAllocator;
}


void MemoryManager::dumpStats() const
{
#ifdef DETAILED_MM_STATS
    std::cerr << "=================" << std::endl;
    std::cerr << "Allocation stats:" << std::endl;
    std::cerr << "Requests for each chunk size:" << std::endl;
    for (int i = 0; i < allocSizeCounters.size(); ++i) {
        if (unsigned count = allocSizeCounters[i]) {
            std::cerr << "\t" << (i << 4) << " bytes chunks: " << count << std::endl;
        }
    }
#endif // DETAILED_MM_STATS
}

#ifdef DETAILED_MM_STATS
void MemoryManager::willAllocate(std::size_t size)
{
    unsigned alignedSize = (size + 15) >> 4;
    QVector<unsigned> &counters = allocSizeCounters;
    if ((unsigned) counters.size() < alignedSize + 1)
        counters.resize(alignedSize + 1);
    counters[alignedSize]++;
}

#endif // DETAILED_MM_STATS

void MemoryManager::collectFromJSStack() const
{
    Value *v = engine->jsStackBase;
    Value *top = engine->jsStackTop;
    while (v < top) {
        Managed *m = v->managed();
        if (m && m->inUse())
            // Skip pointers to already freed objects, they are bogus as well
            m->mark(engine);
        ++v;
    }
}

} // namespace QV4

QT_END_NAMESPACE
