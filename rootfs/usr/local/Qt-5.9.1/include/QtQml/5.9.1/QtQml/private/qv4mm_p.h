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

#ifndef QV4GC_H
#define QV4GC_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qv4global_p.h>
#include <private/qv4value_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4object_p.h>
#include <private/qv4mmdefs_p.h>
#include <QVector>

//#define DETAILED_MM_STATS

#define QV4_MM_MAXBLOCK_SHIFT "QV4_MM_MAXBLOCK_SHIFT"
#define QV4_MM_MAX_CHUNK_SIZE "QV4_MM_MAX_CHUNK_SIZE"
#define QV4_MM_STATS "QV4_MM_STATS"

#define MM_DEBUG 0

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ChunkAllocator;

template<typename T>
struct StackAllocator {
    Q_STATIC_ASSERT(sizeof(T) < Chunk::DataSize);
    static const uint requiredSlots = (sizeof(T) + sizeof(HeapItem) - 1)/sizeof(HeapItem);

    StackAllocator(ChunkAllocator *chunkAlloc);

    T *allocate() {
        T *m = nextFree->as<T>();
        if (Q_UNLIKELY(nextFree == lastInChunk)) {
            nextChunk();
        } else {
            nextFree += requiredSlots;
        }
#if MM_DEBUG
        Chunk *c = m->chunk();
        Chunk::setBit(c->objectBitmap, m - c->realBase());
#endif
        return m;
    }
    void free() {
#if MM_DEBUG
        Chunk::clearBit(item->chunk()->objectBitmap, item - item->chunk()->realBase());
#endif
        if (Q_UNLIKELY(nextFree == firstInChunk)) {
            prevChunk();
        } else {
            nextFree -= requiredSlots;
        }
    }

    void nextChunk();
    void prevChunk();

    void freeAll();

    ChunkAllocator *chunkAllocator;
    HeapItem *nextFree = 0;
    HeapItem *firstInChunk = 0;
    HeapItem *lastInChunk = 0;
    std::vector<Chunk *> chunks;
    uint currentChunk = 0;
};

struct BlockAllocator {
    BlockAllocator(ChunkAllocator *chunkAllocator)
        : chunkAllocator(chunkAllocator)
    {
        memset(freeBins, 0, sizeof(freeBins));
#if MM_DEBUG
        memset(allocations, 0, sizeof(allocations));
#endif
    }

    enum { NumBins = 8 };

    static inline size_t binForSlots(size_t nSlots) {
        return nSlots >= NumBins ? NumBins - 1 : nSlots;
    }

#if MM_DEBUG
    void stats();
#endif

    HeapItem *allocate(size_t size, bool forceAllocation = false);

    size_t totalSlots() const {
        return Chunk::AvailableSlots*chunks.size();
    }

    size_t allocatedMem() const {
        return chunks.size()*Chunk::DataSize;
    }
    size_t usedMem() const {
        uint used = 0;
        for (auto c : chunks)
            used += c->nUsedSlots()*Chunk::SlotSize;
        return used;
    }

    void sweep();
    void freeAll();

    // bump allocations
    HeapItem *nextFree = 0;
    size_t nFree = 0;
    size_t usedSlotsAfterLastSweep = 0;
    HeapItem *freeBins[NumBins];
    ChunkAllocator *chunkAllocator;
    std::vector<Chunk *> chunks;
#if MM_DEBUG
    uint allocations[NumBins];
#endif
};

struct HugeItemAllocator {
    HugeItemAllocator(ChunkAllocator *chunkAllocator)
        : chunkAllocator(chunkAllocator)
    {}

    HeapItem *allocate(size_t size);
    void sweep();
    void freeAll();

    size_t usedMem() const {
        size_t used = 0;
        for (const auto &c : chunks)
            used += c.size;
        return used;
    }

    ChunkAllocator *chunkAllocator;
    struct HugeChunk {
        Chunk *chunk;
        size_t size;
    };

    std::vector<HugeChunk> chunks;
};


class Q_QML_EXPORT MemoryManager
{
    Q_DISABLE_COPY(MemoryManager);

public:
    MemoryManager(ExecutionEngine *engine);
    ~MemoryManager();

    // TODO: this is only for 64bit (and x86 with SSE/AVX), so exend it for other architectures to be slightly more efficient (meaning, align on 8-byte boundaries).
    // Note: all occurrences of "16" in alloc/dealloc are also due to the alignment.
    Q_DECL_CONSTEXPR static inline std::size_t align(std::size_t size)
    { return (size + Chunk::SlotSize - 1) & ~(Chunk::SlotSize - 1); }

    QV4::Heap::CallContext *allocSimpleCallContext()
    {
        Heap::CallContext *ctxt = stackAllocator.allocate();
        memset(ctxt, 0, sizeof(Heap::CallContext));
        ctxt->internalClass = CallContext::defaultInternalClass(engine);
        Q_ASSERT(ctxt->internalClass && ctxt->internalClass->vtable);
        ctxt->init();
        return ctxt;

    }
    void freeSimpleCallContext()
    { stackAllocator.free(); }

    template<typename ManagedType>
    inline typename ManagedType::Data *allocManaged(std::size_t size)
    {
        V4_ASSERT_IS_TRIVIAL(typename ManagedType::Data)
        size = align(size);
        Heap::Base *o = allocData(size);
        InternalClass *ic = ManagedType::defaultInternalClass(engine);
        ic = ic->changeVTable(ManagedType::staticVTable());
        o->internalClass = ic;
        Q_ASSERT(o->internalClass && o->internalClass->vtable);
        return static_cast<typename ManagedType::Data *>(o);
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocateObject(InternalClass *ic)
    {
        Heap::Object *o = allocObjectWithMemberData(ObjectType::staticVTable(), ic->size);
        o->internalClass = ic;
        Q_ASSERT(o->internalClass && o->internalClass->vtable);
        Q_ASSERT(ic->vtable == ObjectType::staticVTable());
        return static_cast<typename ObjectType::Data *>(o);
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocateObject()
    {
        InternalClass *ic = ObjectType::defaultInternalClass(engine);
        ic = ic->changeVTable(ObjectType::staticVTable());
        ic = ic->changePrototype(ObjectType::defaultPrototype(engine)->d());
        Heap::Object *o = allocObjectWithMemberData(ObjectType::staticVTable(), ic->size);
        o->internalClass = ic;
        Q_ASSERT(o->internalClass && o->internalClass->vtable);
        Q_ASSERT(o->internalClass->prototype == ObjectType::defaultPrototype(engine)->d());
        return static_cast<typename ObjectType::Data *>(o);
    }

    template <typename ManagedType, typename Arg1>
    typename ManagedType::Data *allocWithStringData(std::size_t unmanagedSize, Arg1 arg1)
    {
        typename ManagedType::Data *o = reinterpret_cast<typename ManagedType::Data *>(allocString(unmanagedSize));
        o->internalClass = ManagedType::defaultInternalClass(engine);
        Q_ASSERT(o->internalClass && o->internalClass->vtable);
        o->init(arg1);
        return o;
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject(InternalClass *ic)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        Q_ASSERT(t->internalClass()->prototype == (prototype ? prototype->d() : 0));
        Q_UNUSED(prototype);
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType, typename Arg1>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        Q_ASSERT(t->internalClass()->prototype == (prototype ? prototype->d() : 0));
        Q_UNUSED(prototype);
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        Q_ASSERT(t->internalClass()->prototype == (prototype ? prototype->d() : 0));
        Q_UNUSED(prototype);
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        Q_ASSERT(t->internalClass()->prototype == (prototype ? prototype->d() : 0));
        Q_UNUSED(prototype);
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ObjectType::Data *allocObject(InternalClass *ic, Object *prototype, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>(ic));
        Q_ASSERT(t->internalClass()->prototype == (prototype ? prototype->d() : 0));
        Q_UNUSED(prototype);
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }

    template <typename ObjectType>
    typename ObjectType::Data *allocObject()
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ObjectType, typename Arg1>
    typename ObjectType::Data *allocObject(Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ObjectType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ObjectType::Data *allocObject(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ObjectType> t(scope, allocateObject<ObjectType>());
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }


    template <typename ManagedType>
    typename ManagedType::Data *alloc()
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init();
        return t->d();
    }

    template <typename ManagedType, typename Arg1>
    typename ManagedType::Data *alloc(Arg1 arg1)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3, arg4);
        return t->d();
    }

    template <typename ManagedType, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    typename ManagedType::Data *alloc(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        Scope scope(engine);
        Scoped<ManagedType> t(scope, allocManaged<ManagedType>(sizeof(typename ManagedType::Data)));
        t->d_unchecked()->init(arg1, arg2, arg3, arg4, arg5);
        return t->d();
    }

    void runGC();

    void dumpStats() const;

    size_t getUsedMem() const;
    size_t getAllocatedMem() const;
    size_t getLargeItemsMem() const;

    // called when a JS object grows itself. Specifically: Heap::String::append
    void changeUnmanagedHeapSizeUsage(qptrdiff delta) { unmanagedHeapSize += delta; }


protected:
    /// expects size to be aligned
    Heap::Base *allocString(std::size_t unmanagedSize);
    Heap::Base *allocData(std::size_t size);
    Heap::Object *allocObjectWithMemberData(const QV4::VTable *vtable, uint nMembers);

#ifdef DETAILED_MM_STATS
    void willAllocate(std::size_t size);
#endif // DETAILED_MM_STATS

private:
    void collectFromJSStack() const;
    void mark();
    void sweep(bool lastSweep = false);
    bool shouldRunGC() const;

public:
    QV4::ExecutionEngine *engine;
    ChunkAllocator *chunkAllocator;
    StackAllocator<Heap::CallContext> stackAllocator;
    BlockAllocator blockAllocator;
    HugeItemAllocator hugeItemAllocator;
    PersistentValueStorage *m_persistentValues;
    PersistentValueStorage *m_weakValues;
    QVector<Value *> m_pendingFreedObjectWrapperValue;

    std::size_t unmanagedHeapSize = 0; // the amount of bytes of heap that is not managed by the memory manager, but which is held onto by managed items.
    std::size_t unmanagedHeapSizeGCLimit;

    bool gcBlocked = false;
    bool aggressiveGC = false;
    bool gcStats = false;
};

}

QT_END_NAMESPACE

#endif // QV4GC_H
