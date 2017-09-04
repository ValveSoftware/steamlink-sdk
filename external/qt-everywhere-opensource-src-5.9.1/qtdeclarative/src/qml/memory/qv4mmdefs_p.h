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
#ifndef QV4MMDEFS_P_H
#define QV4MMDEFS_P_H

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
#include <private/qv4runtimeapi_p.h>
#include <QtCore/qalgorithms.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct MarkStack;

typedef void(*ClassDestroyStatsCallback)(const char *);

/*
 * Chunks are the basic structure containing GC managed objects.
 *
 * Chunks are 64k aligned in memory, so that retrieving the Chunk pointer from a Heap object
 * is a simple masking operation. Each Chunk has 4 bitmaps for managing purposes,
 * and 32byte wide slots for the objects following afterwards.
 *
 * The gray and black bitmaps are used for mark/sweep.
 * The object bitmap has a bit set if this location represents the start of a Heap object.
 * The extends bitmap denotes the extend of an object. It has a cleared bit at the start of the object
 * and a set bit for all following slots used by the object.
 *
 * Free memory has both used and extends bits set to 0.
 *
 * This gives the following operations when allocating an object of size s:
 * Find s/Alignment consecutive free slots in the chunk. Set the object bit for the first
 * slot to 1. Set the extends bits for all following slots to 1.
 *
 * All used slots can be found by object|extents.
 *
 * When sweeping, simply copy the black bits over to the object bits.
 *
 */
struct HeapItem;
struct Chunk {
    enum {
        ChunkSize = 64*1024,
        ChunkShift = 16,
        SlotSize = 32,
        SlotSizeShift = 5,
        NumSlots = ChunkSize/SlotSize,
        BitmapSize = NumSlots/8,
        HeaderSize = 4*BitmapSize,
        DataSize = ChunkSize - HeaderSize,
        AvailableSlots = DataSize/SlotSize,
#if QT_POINTER_SIZE == 8
        Bits = 64,
        BitShift = 6,
#else
        Bits = 32,
        BitShift = 5,
#endif
        EntriesInBitmap = BitmapSize/sizeof(quintptr)
    };
    quintptr grayBitmap[BitmapSize/sizeof(quintptr)];
    quintptr blackBitmap[BitmapSize/sizeof(quintptr)];
    quintptr objectBitmap[BitmapSize/sizeof(quintptr)];
    quintptr extendsBitmap[BitmapSize/sizeof(quintptr)];
    char data[ChunkSize - HeaderSize];

    HeapItem *realBase();
    HeapItem *first();

    static void setBit(quintptr *bitmap, size_t index) {
//        Q_ASSERT(index >= HeaderSize/SlotSize && index < ChunkSize/SlotSize);
        bitmap += index >> BitShift;
        quintptr bit = static_cast<quintptr>(1) << (index & (Bits - 1));
        *bitmap |= bit;
    }
    static void clearBit(quintptr *bitmap, size_t index) {
//        Q_ASSERT(index >= HeaderSize/SlotSize && index < ChunkSize/SlotSize);
        bitmap += index >> BitShift;
        quintptr bit = static_cast<quintptr>(1) << (index & (Bits - 1));
        *bitmap &= ~bit;
    }
    static bool testBit(quintptr *bitmap, size_t index) {
//        Q_ASSERT(index >= HeaderSize/SlotSize && index < ChunkSize/SlotSize);
        bitmap += index >> BitShift;
        quintptr bit = static_cast<quintptr>(1) << (index & (Bits - 1));
        return (*bitmap & bit);
    }
    static void setBits(quintptr *bitmap, size_t index, size_t nBits) {
//        Q_ASSERT(index >= HeaderSize/SlotSize && index + nBits <= ChunkSize/SlotSize);
        if (!nBits)
            return;
        bitmap += index >> BitShift;
        index &= (Bits - 1);
        while (1) {
            size_t bitsToSet = qMin(nBits, Bits - index);
            quintptr mask = static_cast<quintptr>(-1) >> (Bits - bitsToSet) << index;
            *bitmap |= mask;
            nBits -= bitsToSet;
            if (!nBits)
                return;
            index = 0;
            ++bitmap;
        }
    }
    static bool hasNonZeroBit(quintptr *bitmap) {
        for (uint i = 0; i < EntriesInBitmap; ++i)
            if (bitmap[i])
                return true;
        return false;
    }
    static uint lowestNonZeroBit(quintptr *bitmap) {
        for (uint i = 0; i < EntriesInBitmap; ++i) {
            if (bitmap[i]) {
                quintptr b = bitmap[i];
                return i*Bits + qCountTrailingZeroBits(b);
            }
        }
        return 0;
    }

    uint nFreeSlots() const {
        return AvailableSlots - nUsedSlots();
    }
    uint nUsedSlots() const {
        uint usedSlots = 0;
        for (uint i = 0; i < EntriesInBitmap; ++i) {
            quintptr used = objectBitmap[i] | extendsBitmap[i];
            usedSlots += qPopulationCount(used);
        }
        return usedSlots;
    }

    bool sweep();
    void freeAll();

    void sortIntoBins(HeapItem **bins, uint nBins);
};

struct HeapItem {
    union {
        struct {
            HeapItem *next;
            size_t availableSlots;
        } freeData;
        quint64 payload[Chunk::SlotSize/sizeof(quint64)];
    };
    operator Heap::Base *() { return reinterpret_cast<Heap::Base *>(this); }

    template<typename T>
    T *as() { return static_cast<T *>(reinterpret_cast<Heap::Base *>(this)); }

    Chunk *chunk() const {
        return reinterpret_cast<Chunk *>(reinterpret_cast<quintptr>(this) >> Chunk::ChunkShift << Chunk::ChunkShift);
    }

    bool isGray() const {
        Chunk *c = chunk();
        uint index = this - c->realBase();
        return Chunk::testBit(c->grayBitmap, index);
    }
    bool isBlack() const {
        Chunk *c = chunk();
        uint index = this - c->realBase();
        return Chunk::testBit(c->blackBitmap, index);
    }
    bool isInUse() const {
        Chunk *c = chunk();
        uint index = this - c->realBase();
        return Chunk::testBit(c->objectBitmap, index);
    }

    void setAllocatedSlots(size_t nSlots) {
//        Q_ASSERT(size && !(size % sizeof(HeapItem)));
        Chunk *c = chunk();
        size_t index = this - c->realBase();
//        Q_ASSERT(!Chunk::testBit(c->objectBitmap, index));
        Chunk::setBit(c->objectBitmap, index);
        Chunk::setBits(c->extendsBitmap, index + 1, nSlots - 1);
//        for (uint i = index + 1; i < nBits - 1; ++i)
//            Q_ASSERT(Chunk::testBit(c->extendsBitmap, i));
//        Q_ASSERT(!Chunk::testBit(c->extendsBitmap, index));
    }

    // Doesn't report correctly for huge items
    size_t size() const {
        Chunk *c = chunk();
        uint index = this - c->realBase();
        Q_ASSERT(Chunk::testBit(c->objectBitmap, index));
        // ### optimize me
        uint end = index + 1;
        while (end < Chunk::NumSlots && Chunk::testBit(c->extendsBitmap, end))
            ++end;
        return (end - index)*sizeof(HeapItem);
    }
};

inline HeapItem *Chunk::realBase()
{
    return reinterpret_cast<HeapItem *>(this);
}

inline HeapItem *Chunk::first()
{
    return reinterpret_cast<HeapItem *>(data);
}

Q_STATIC_ASSERT(sizeof(Chunk) == Chunk::ChunkSize);
Q_STATIC_ASSERT((1 << Chunk::ChunkShift) == Chunk::ChunkSize);
Q_STATIC_ASSERT(1 << Chunk::SlotSizeShift == Chunk::SlotSize);
Q_STATIC_ASSERT(sizeof(HeapItem) == Chunk::SlotSize);
Q_STATIC_ASSERT(QT_POINTER_SIZE*8 == Chunk::Bits);
Q_STATIC_ASSERT((1 << Chunk::BitShift) == Chunk::Bits);

}

QT_END_NAMESPACE

#endif
