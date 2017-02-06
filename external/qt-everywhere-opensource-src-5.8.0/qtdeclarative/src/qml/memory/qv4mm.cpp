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
#include <qqmlengine.h>
#include "PageAllocation.h"
#include "StdLibExtras.h"

#include <QElapsedTimer>
#include <QMap>
#include <QScopedValueRollback>

#include <iostream>
#include <cstdlib>
#include <algorithm>
#include "qv4alloca_p.h"
#include "qv4profiling_p.h"

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

static uint maxShiftValue()
{
    static uint result = 0;
    if (!result) {
        result = 6;
        if (Q_UNLIKELY(qEnvironmentVariableIsSet(QV4_MM_MAXBLOCK_SHIFT))) {
            bool ok;
            const uint overrideValue = qgetenv(QV4_MM_MAXBLOCK_SHIFT).toUInt(&ok);
            if (ok && overrideValue <= 11 && overrideValue > 0)
                result = overrideValue;
        }
    }
    return result;
}

static std::size_t maxChunkSizeValue()
{
    static std::size_t result = 0;
    if (!result) {
        result = 32 * 1024;
        if (Q_UNLIKELY(qEnvironmentVariableIsSet(QV4_MM_MAX_CHUNK_SIZE))) {
            bool ok;
            const std::size_t overrideValue = qgetenv(QV4_MM_MAX_CHUNK_SIZE).toUInt(&ok);
            if (ok)
                result = overrideValue;
        }
    }
    return result;
}

using namespace QV4;

struct MemoryManager::Data
{
    const size_t pageSize;

    struct ChunkHeader {
        Heap::Base freeItems;
        ChunkHeader *nextNonFull;
        char *itemStart;
        char *itemEnd;
        unsigned itemSize;
    };

    ExecutionEngine *engine;

    std::size_t maxChunkSize;
    std::vector<PageAllocation> heapChunks;
    std::size_t unmanagedHeapSize; // the amount of bytes of heap that is not managed by the memory manager, but which is held onto by managed items.
    std::size_t unmanagedHeapSizeGCLimit;

    struct LargeItem {
        LargeItem *next;
        size_t size;
        void *data;

        Heap::Base *heapObject() {
            return reinterpret_cast<Heap::Base *>(&data);
        }
    };

    LargeItem *largeItems;
    std::size_t totalLargeItemsAllocated;

    enum { MaxItemSize = 512 };
    ChunkHeader *nonFullChunks[MaxItemSize/16];
    uint nChunks[MaxItemSize/16];
    uint availableItems[MaxItemSize/16];
    uint allocCount[MaxItemSize/16];
    int totalItems;
    int totalAlloc;
    uint maxShift;

    bool gcBlocked;
    bool aggressiveGC;
    bool gcStats;
    bool unused; // suppress padding warning

    // statistics:
#ifdef DETAILED_MM_STATS
    QVector<unsigned> allocSizeCounters;
#endif // DETAILED_MM_STATS

    Data()
        : pageSize(WTF::pageSize())
        , engine(0)
        , maxChunkSize(maxChunkSizeValue())
        , unmanagedHeapSize(0)
        , unmanagedHeapSizeGCLimit(MIN_UNMANAGED_HEAPSIZE_GC_LIMIT)
        , largeItems(0)
        , totalLargeItemsAllocated(0)
        , totalItems(0)
        , totalAlloc(0)
        , maxShift(maxShiftValue())
        , gcBlocked(false)
        , aggressiveGC(!qEnvironmentVariableIsEmpty("QV4_MM_AGGRESSIVE_GC"))
        , gcStats(!qEnvironmentVariableIsEmpty(QV4_MM_STATS))
    {
        memset(nonFullChunks, 0, sizeof(nonFullChunks));
        memset(nChunks, 0, sizeof(nChunks));
        memset(availableItems, 0, sizeof(availableItems));
        memset(allocCount, 0, sizeof(allocCount));
    }

    ~Data()
    {
        for (std::vector<PageAllocation>::iterator i = heapChunks.begin(), ei = heapChunks.end(); i != ei; ++i) {
            Q_V4_PROFILE_DEALLOC(engine, i->size(), Profiling::HeapPage);
            i->deallocate();
        }
    }
};

namespace {

bool sweepChunk(MemoryManager::Data::ChunkHeader *header, uint *itemsInUse, ExecutionEngine *engine, std::size_t *unmanagedHeapSize)
{
    Q_ASSERT(unmanagedHeapSize);

    bool isEmpty = true;
    Heap::Base *tail = &header->freeItems;
//    qDebug("chunkStart @ %p, size=%x, pos=%x", header->itemStart, header->itemSize, header->itemSize>>4);
#ifdef V4_USE_VALGRIND
    VALGRIND_DISABLE_ERROR_REPORTING;
#endif
    for (char *item = header->itemStart; item <= header->itemEnd; item += header->itemSize) {
        Heap::Base *m = reinterpret_cast<Heap::Base *>(item);
//        qDebug("chunk @ %p, in use: %s, mark bit: %s",
//               item, (m->inUse() ? "yes" : "no"), (m->isMarked() ? "true" : "false"));

        Q_ASSERT(qintptr(item) % 16 == 0);

        if (m->isMarked()) {
            Q_ASSERT(m->inUse());
            m->clearMarkBit();
            isEmpty = false;
            ++(*itemsInUse);
        } else {
            if (m->inUse()) {
//                qDebug() << "-- collecting it." << m << tail << m->nextFree();
#ifdef V4_USE_VALGRIND
                VALGRIND_ENABLE_ERROR_REPORTING;
#endif
                if (std::size_t(header->itemSize) == MemoryManager::align(sizeof(Heap::String)) && m->vtable()->isString) {
                    std::size_t heapBytes = static_cast<Heap::String *>(m)->retainedTextSize();
                    Q_ASSERT(*unmanagedHeapSize >= heapBytes);
//                    qDebug() << "-- it's a string holding on to" << heapBytes << "bytes";
                    *unmanagedHeapSize -= heapBytes;
                }

                if (m->vtable()->destroy) {
                    m->vtable()->destroy(m);
                    m->_checkIsDestroyed();
                }

                memset(m, 0, sizeof(Heap::Base));
#ifdef V4_USE_VALGRIND
                VALGRIND_DISABLE_ERROR_REPORTING;
                VALGRIND_MEMPOOL_FREE(engine->memoryManager, m);
#endif
#ifdef V4_USE_HEAPTRACK
                heaptrack_report_free(m);
#endif
                Q_V4_PROFILE_DEALLOC(engine, header->itemSize, Profiling::SmallItem);
                ++(*itemsInUse);
            }
            // Relink all free blocks to rewrite references to any released chunk.
            tail->setNextFree(m);
            tail = m;
        }
    }
    tail->setNextFree(0);
#ifdef V4_USE_VALGRIND
    VALGRIND_ENABLE_ERROR_REPORTING;
#endif
    return isEmpty;
}

} // namespace

MemoryManager::MemoryManager(ExecutionEngine *engine)
    : engine(engine)
    , m_d(new Data)
    , m_persistentValues(new PersistentValueStorage(engine))
    , m_weakValues(new PersistentValueStorage(engine))
{
#ifdef V4_USE_VALGRIND
    VALGRIND_CREATE_MEMPOOL(this, 0, true);
#endif
    m_d->engine = engine;
}

Heap::Base *MemoryManager::allocData(std::size_t size, std::size_t unmanagedSize)
{
    if (m_d->aggressiveGC)
        runGC();
#ifdef DETAILED_MM_STATS
    willAllocate(size);
#endif // DETAILED_MM_STATS

    Q_ASSERT(size >= 16);
    Q_ASSERT(size % 16 == 0);

//    qDebug() << "unmanagedHeapSize:" << m_d->unmanagedHeapSize << "limit:" << m_d->unmanagedHeapSizeGCLimit << "unmanagedSize:" << unmanagedSize;
    m_d->unmanagedHeapSize += unmanagedSize;
    bool didGCRun = false;
    if (m_d->unmanagedHeapSize > m_d->unmanagedHeapSizeGCLimit) {
        runGC();

        if (3*m_d->unmanagedHeapSizeGCLimit <= 4*m_d->unmanagedHeapSize)
            // more than 75% full, raise limit
            m_d->unmanagedHeapSizeGCLimit = std::max(m_d->unmanagedHeapSizeGCLimit, m_d->unmanagedHeapSize) * 2;
        else if (m_d->unmanagedHeapSize * 4 <= m_d->unmanagedHeapSizeGCLimit)
            // less than 25% full, lower limit
            m_d->unmanagedHeapSizeGCLimit = qMax(MIN_UNMANAGED_HEAPSIZE_GC_LIMIT, m_d->unmanagedHeapSizeGCLimit/2);
        didGCRun = true;
    }

    size_t pos = size >> 4;

    // doesn't fit into a small bucket
    if (size >= MemoryManager::Data::MaxItemSize) {
        if (!didGCRun && m_d->totalLargeItemsAllocated > 8 * 1024 * 1024)
            runGC();

        // we use malloc for this
        const size_t totalSize = size + sizeof(MemoryManager::Data::LargeItem);
        Q_V4_PROFILE_ALLOC(engine, totalSize, Profiling::LargeItem);
        MemoryManager::Data::LargeItem *item =
                static_cast<MemoryManager::Data::LargeItem *>(malloc(totalSize));
        memset(item, 0, totalSize);
        item->next = m_d->largeItems;
        item->size = size;
        m_d->largeItems = item;
        m_d->totalLargeItemsAllocated += size;
        return item->heapObject();
    }

    Heap::Base *m = 0;
    Data::ChunkHeader *header = m_d->nonFullChunks[pos];
    if (header) {
        m = header->freeItems.nextFree();
        goto found;
    }

    // try to free up space, otherwise allocate
    if (!didGCRun && m_d->allocCount[pos] > (m_d->availableItems[pos] >> 1) && m_d->totalAlloc > (m_d->totalItems >> 1) && !m_d->aggressiveGC) {
        runGC();
        header = m_d->nonFullChunks[pos];
        if (header) {
            m = header->freeItems.nextFree();
            goto found;
        }
    }

    // no free item available, allocate a new chunk
    {
        // allocate larger chunks at a time to avoid excessive GC, but cap at maximum chunk size (2MB by default)
        uint shift = ++m_d->nChunks[pos];
        if (shift > m_d->maxShift)
            shift = m_d->maxShift;
        std::size_t allocSize = m_d->maxChunkSize*(size_t(1) << shift);
        allocSize = roundUpToMultipleOf(m_d->pageSize, allocSize);
        Q_V4_PROFILE_ALLOC(engine, allocSize, Profiling::HeapPage);
        PageAllocation allocation = PageAllocation::allocate(allocSize, OSAllocator::JSGCHeapPages);
        m_d->heapChunks.push_back(allocation);

        header = reinterpret_cast<Data::ChunkHeader *>(allocation.base());
        Q_ASSERT(size <= UINT_MAX);
        header->itemSize = unsigned(size);
        header->itemStart = reinterpret_cast<char *>(allocation.base()) + roundUpToMultipleOf(16, sizeof(Data::ChunkHeader));
        header->itemEnd = reinterpret_cast<char *>(allocation.base()) + allocation.size() - header->itemSize;

        header->nextNonFull = m_d->nonFullChunks[pos];
        m_d->nonFullChunks[pos] = header;

        Heap::Base *last = &header->freeItems;
        for (char *item = header->itemStart; item <= header->itemEnd; item += header->itemSize) {
            Heap::Base *o = reinterpret_cast<Heap::Base *>(item);
            last->setNextFree(o);
            last = o;

        }
        last->setNextFree(0);
        m = header->freeItems.nextFree();
        Q_ASSERT(header->itemEnd >= header->itemStart);
        const size_t increase = quintptr(header->itemEnd - header->itemStart) / header->itemSize;
        m_d->availableItems[pos] += uint(increase);
        m_d->totalItems += int(increase);
#ifdef V4_USE_VALGRIND
        VALGRIND_MAKE_MEM_NOACCESS(allocation.base(), allocSize);
        VALGRIND_MEMPOOL_ALLOC(this, header, sizeof(Data::ChunkHeader));
#endif
#ifdef V4_USE_HEAPTRACK
        heaptrack_report_alloc(header, sizeof(Data::ChunkHeader));
#endif
    }

  found:
#ifdef V4_USE_VALGRIND
    VALGRIND_MEMPOOL_ALLOC(this, m, size);
#endif
#ifdef V4_USE_HEAPTRACK
    heaptrack_report_alloc(m, size);
#endif
    Q_V4_PROFILE_ALLOC(engine, size, Profiling::SmallItem);

    ++m_d->allocCount[pos];
    ++m_d->totalAlloc;
    header->freeItems.setNextFree(m->nextFree());
    if (!header->freeItems.nextFree())
        m_d->nonFullChunks[pos] = header->nextNonFull;
    return m;
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
        if (!(*it).isManaged())
            continue;
        if (!(*it).as<QObjectWrapper>())
            continue;
        QObjectWrapper *qobjectWrapper = static_cast<QObjectWrapper*>((*it).managed());
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
        if (!(*it).isManaged())
            continue;
        Managed *m = (*it).managed();
        if (m->markBit())
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
        if (!(*it).isManaged())
            continue;
        Managed *m = (*it).as<Managed>();
        if (m->markBit())
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

    bool *chunkIsEmpty = static_cast<bool *>(alloca(m_d->heapChunks.size() * sizeof(bool)));
    uint itemsInUse[MemoryManager::Data::MaxItemSize/16];
    memset(itemsInUse, 0, sizeof(itemsInUse));
    memset(m_d->nonFullChunks, 0, sizeof(m_d->nonFullChunks));

    for (size_t i = 0; i < m_d->heapChunks.size(); ++i) {
        Data::ChunkHeader *header = reinterpret_cast<Data::ChunkHeader *>(m_d->heapChunks[i].base());
        chunkIsEmpty[i] = sweepChunk(header, &itemsInUse[header->itemSize >> 4], engine, &m_d->unmanagedHeapSize);
    }

    std::vector<PageAllocation>::iterator chunkIter = m_d->heapChunks.begin();
    for (size_t i = 0; i < m_d->heapChunks.size(); ++i) {
        Q_ASSERT(chunkIter != m_d->heapChunks.end());
        Data::ChunkHeader *header = reinterpret_cast<Data::ChunkHeader *>(chunkIter->base());
        const size_t pos = header->itemSize >> 4;
        Q_ASSERT(header->itemEnd >= header->itemStart);
        const size_t decrease = quintptr(header->itemEnd - header->itemStart) / header->itemSize;

        // Release that chunk if it could have been spared since the last GC run without any difference.
        if (chunkIsEmpty[i] && m_d->availableItems[pos] - decrease >= itemsInUse[pos]) {
            Q_V4_PROFILE_DEALLOC(engine, chunkIter->size(), Profiling::HeapPage);
#ifdef V4_USE_VALGRIND
            VALGRIND_MEMPOOL_FREE(this, header);
#endif
#ifdef V4_USE_HEAPTRACK
            heaptrack_report_free(header);
#endif
            --m_d->nChunks[pos];
            m_d->availableItems[pos] -= uint(decrease);
            m_d->totalItems -= int(decrease);
            chunkIter->deallocate();
            chunkIter = m_d->heapChunks.erase(chunkIter);
            continue;
        } else if (header->freeItems.nextFree()) {
            header->nextNonFull = m_d->nonFullChunks[pos];
            m_d->nonFullChunks[pos] = header;
        }
        ++chunkIter;
    }

    Data::LargeItem *i = m_d->largeItems;
    Data::LargeItem **last = &m_d->largeItems;
    while (i) {
        Heap::Base *m = i->heapObject();
        Q_ASSERT(m->inUse());
        if (m->isMarked()) {
            m->clearMarkBit();
            last = &i->next;
            i = i->next;
            continue;
        }
        if (m->vtable()->destroy)
            m->vtable()->destroy(m);

        *last = i->next;
        Q_V4_PROFILE_DEALLOC(engine, i->size + sizeof(Data::LargeItem), Profiling::LargeItem);
        free(i);
        i = *last;
    }

    // some execution contexts are allocated on the stack, make sure we clear their markBit as well
    if (!lastSweep) {
        QV4::ExecutionContext *ctx = engine->currentContext;
        while (ctx) {
            ctx->d()->clearMarkBit();
            ctx = engine->parentContext(ctx);
        }
    }
}

bool MemoryManager::isGCBlocked() const
{
    return m_d->gcBlocked;
}

void MemoryManager::setGCBlocked(bool blockGC)
{
    m_d->gcBlocked = blockGC;
}

void MemoryManager::runGC()
{
    if (m_d->gcBlocked) {
//        qDebug() << "Not running GC.";
        return;
    }

    QScopedValueRollback<bool> gcBlocker(m_d->gcBlocked, true);

    if (!m_d->gcStats) {
        mark();
        sweep();
    } else {
        const size_t totalMem = getAllocatedMem();

        QElapsedTimer t;
        t.start();
        mark();
        qint64 markTime = t.restart();
        const size_t usedBefore = getUsedMem();
        const size_t largeItemsBefore = getLargeItemsMem();
        size_t chunksBefore = m_d->heapChunks.size();
        sweep();
        const size_t usedAfter = getUsedMem();
        const size_t largeItemsAfter = getLargeItemsMem();
        qint64 sweepTime = t.elapsed();

        qDebug() << "========== GC ==========";
        qDebug() << "Marked object in" << markTime << "ms.";
        qDebug() << "Sweeped object in" << sweepTime << "ms.";
        qDebug() << "Allocated" << totalMem << "bytes in" << m_d->heapChunks.size() << "chunks.";
        qDebug() << "Used memory before GC:" << usedBefore;
        qDebug() << "Used memory after GC:" << usedAfter;
        qDebug() << "Freed up bytes:" << (usedBefore - usedAfter);
        qDebug() << "Released chunks:" << (chunksBefore - m_d->heapChunks.size());
        qDebug() << "Large item memory before GC:" << largeItemsBefore;
        qDebug() << "Large item memory after GC:" << largeItemsAfter;
        qDebug() << "Large item memory freed up:" << (largeItemsBefore - largeItemsAfter);
        qDebug() << "======== End GC ========";
    }

    memset(m_d->allocCount, 0, sizeof(m_d->allocCount));
    m_d->totalAlloc = 0;
    m_d->totalLargeItemsAllocated = 0;
}

size_t MemoryManager::getUsedMem() const
{
    size_t usedMem = 0;
    for (std::vector<PageAllocation>::const_iterator i = m_d->heapChunks.cbegin(), ei = m_d->heapChunks.cend(); i != ei; ++i) {
        Data::ChunkHeader *header = reinterpret_cast<Data::ChunkHeader *>(i->base());
        for (char *item = header->itemStart; item <= header->itemEnd; item += header->itemSize) {
            Heap::Base *m = reinterpret_cast<Heap::Base *>(item);
            Q_ASSERT(qintptr(item) % 16 == 0);
            if (m->inUse())
                usedMem += header->itemSize;
        }
    }
    return usedMem;
}

size_t MemoryManager::getAllocatedMem() const
{
    size_t total = 0;
    for (size_t i = 0; i < m_d->heapChunks.size(); ++i)
        total += m_d->heapChunks.at(i).size();
    return total;
}

size_t MemoryManager::getLargeItemsMem() const
{
    size_t total = 0;
    for (const Data::LargeItem *i = m_d->largeItems; i != 0; i = i->next)
        total += i->size;
    return total;
}

void MemoryManager::growUnmanagedHeapSizeUsage(size_t delta)
{
    m_d->unmanagedHeapSize += delta;
}

MemoryManager::~MemoryManager()
{
    delete m_persistentValues;

    sweep(/*lastSweep*/true);

    delete m_weakValues;
#ifdef V4_USE_VALGRIND
    VALGRIND_DESTROY_MEMPOOL(this);
#endif
}



void MemoryManager::dumpStats() const
{
#ifdef DETAILED_MM_STATS
    std::cerr << "=================" << std::endl;
    std::cerr << "Allocation stats:" << std::endl;
    std::cerr << "Requests for each chunk size:" << std::endl;
    for (int i = 0; i < m_d->allocSizeCounters.size(); ++i) {
        if (unsigned count = m_d->allocSizeCounters[i]) {
            std::cerr << "\t" << (i << 4) << " bytes chunks: " << count << std::endl;
        }
    }
#endif // DETAILED_MM_STATS
}

#ifdef DETAILED_MM_STATS
void MemoryManager::willAllocate(std::size_t size)
{
    unsigned alignedSize = (size + 15) >> 4;
    QVector<unsigned> &counters = m_d->allocSizeCounters;
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
QT_END_NAMESPACE
