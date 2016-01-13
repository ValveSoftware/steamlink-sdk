/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4executableallocator_p.h"

#include <wtf/StdLibExtras.h>
#include <wtf/PageAllocation.h>

using namespace QV4;

void *ExecutableAllocator::Allocation::start() const
{
    return reinterpret_cast<void*>(addr);
}

void ExecutableAllocator::Allocation::deallocate(ExecutableAllocator *allocator)
{
    if (isValid())
        allocator->free(this);
    else
        delete this;
}

ExecutableAllocator::Allocation *ExecutableAllocator::Allocation::split(size_t dividingSize)
{
    Allocation *remainder = new Allocation;
    if (next)
        next->prev = remainder;

    remainder->next = next;
    next = remainder;

    remainder->prev = this;

    remainder->size = size - dividingSize;
    remainder->free = free;
    remainder->addr = addr + dividingSize;
    size = dividingSize;

    return remainder;
}

bool ExecutableAllocator::Allocation::mergeNext(ExecutableAllocator *allocator)
{
    Q_ASSERT(free);
    if (!next || !next->free)
        return false;

    allocator->freeAllocations.remove(size, this);
    allocator->freeAllocations.remove(next->size, next);

    size += next->size;
    Allocation *newNext = next->next;
    delete next;
    next = newNext;
    if (next)
        next->prev = this;

    allocator->freeAllocations.insert(size, this);
    return true;
}

bool ExecutableAllocator::Allocation::mergePrevious(ExecutableAllocator *allocator)
{
    Q_ASSERT(free);
    if (!prev || !prev->free)
        return false;

    allocator->freeAllocations.remove(size, this);
    allocator->freeAllocations.remove(prev->size, prev);

    prev->size += size;
    if (next)
        next->prev = prev;
    prev->next = next;

    allocator->freeAllocations.insert(prev->size, prev);

    delete this;
    return true;
}

ExecutableAllocator::ChunkOfPages::~ChunkOfPages()
{
    Allocation *alloc = firstAllocation;
    while (alloc) {
        Allocation *next = alloc->next;
        if (alloc->isValid())
            delete alloc;
        alloc = next;
    }
    pages->deallocate();
    delete pages;
}

bool ExecutableAllocator::ChunkOfPages::contains(Allocation *alloc) const
{
    Allocation *it = firstAllocation;
    while (it) {
        if (it == alloc)
            return true;
        it = it->next;
    }
    return false;
}

ExecutableAllocator::ExecutableAllocator()
    : mutex(QMutex::NonRecursive)
{
}

ExecutableAllocator::~ExecutableAllocator()
{
    foreach (ChunkOfPages *chunk, chunks) {
        for (Allocation *allocation = chunk->firstAllocation; allocation; allocation = allocation->next)
            if (!allocation->free)
                allocation->invalidate();
    }

    qDeleteAll(chunks);
}

ExecutableAllocator::Allocation *ExecutableAllocator::allocate(size_t size)
{
    QMutexLocker locker(&mutex);
    Allocation *allocation = 0;

    // Code is best aligned to 16-byte boundaries.
    size = WTF::roundUpToMultipleOf(16, size);

    QMultiMap<size_t, Allocation*>::Iterator it = freeAllocations.lowerBound(size);
    if (it != freeAllocations.end()) {
        allocation = *it;
        freeAllocations.erase(it);
    }

    if (!allocation) {
        ChunkOfPages *chunk = new ChunkOfPages;
        size_t allocSize = WTF::roundUpToMultipleOf(WTF::pageSize(), size);
        chunk->pages = new WTF::PageAllocation(WTF::PageAllocation::allocate(allocSize, OSAllocator::JSJITCodePages));
        chunks.insert(reinterpret_cast<quintptr>(chunk->pages->base()) - 1, chunk);
        allocation = new Allocation;
        allocation->addr = reinterpret_cast<quintptr>(chunk->pages->base());
        allocation->size = allocSize;
        allocation->free = true;
        chunk->firstAllocation = allocation;
    }

    Q_ASSERT(allocation);
    Q_ASSERT(allocation->free);

    allocation->free = false;

    if (allocation->size > size) {
        Allocation *remainder = allocation->split(size);
        remainder->free = true;
        if (!remainder->mergeNext(this))
            freeAllocations.insert(remainder->size, remainder);
    }

    return allocation;
}

void ExecutableAllocator::free(Allocation *allocation)
{
    QMutexLocker locker(&mutex);

    Q_ASSERT(allocation);

    allocation->free = true;

    QMap<quintptr, ChunkOfPages*>::Iterator it = chunks.lowerBound(allocation->addr);
    if (it != chunks.begin())
        --it;
    Q_ASSERT(it != chunks.end());
    ChunkOfPages *chunk = *it;
    Q_ASSERT(chunk->contains(allocation));

    bool merged = allocation->mergeNext(this);
    merged |= allocation->mergePrevious(this);
    if (!merged)
        freeAllocations.insert(allocation->size, allocation);

    allocation = 0;

    if (!chunk->firstAllocation->next) {
        freeAllocations.remove(chunk->firstAllocation->size, chunk->firstAllocation);
        chunks.erase(it);
        delete chunk;
        return;
    }
}

ExecutableAllocator::ChunkOfPages *ExecutableAllocator::chunkForAllocation(Allocation *allocation) const
{
    QMutexLocker locker(&mutex);

    QMap<quintptr, ChunkOfPages*>::ConstIterator it = chunks.lowerBound(allocation->addr);
    if (it != chunks.begin())
        --it;
    if (it == chunks.end())
        return 0;
    return *it;
}

