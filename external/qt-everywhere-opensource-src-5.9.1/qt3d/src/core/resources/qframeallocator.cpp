/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

/* !\internal
    \class Qt3DCore::QFrameAllocator
    \inmodule Qt3DCore
    \brief Provides a pool of memory chunks to be used to allocate objects on a per frame basis.

    The memory can be recycled by following frames by calling clear which won't deallocate any memory.

    \note Be really careful when allocating polymorphic types. You must be
    sure to call deallocate with the subclass type to properly release all
    memory.
*/

#include "qframeallocator_p.h"
#include "qframeallocator_p_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QFrameAllocatorPrivate::QFrameAllocatorPrivate()
{
}

QFrameAllocator::QFrameAllocator(uint maxObjectSize, uint alignment, uint pageSize)
    : d_ptr(new QFrameAllocatorPrivate)
{
    Q_ASSERT(alignment && pageSize && pageSize < UCHAR_MAX);
    Q_D(QFrameAllocator);
    d->m_maxObjectSize = maxObjectSize;
    d->m_alignment = alignment;
    d->m_allocatorPool.resize(d->allocatorIndexFromSize(maxObjectSize) + 1);
    for (int i = 0, n = d->m_allocatorPool.size(); i < n; ++i)
        d->m_allocatorPool[i].init((i + 1) * d->m_alignment, pageSize);
}

QFrameAllocator::~QFrameAllocator()
{
    Q_D(QFrameAllocator);
    for (int i = 0, n = d->m_allocatorPool.size(); i < n; ++i)
        d->m_allocatorPool[i].release();
}

// Clear all memory chunks, allocated memory is not released
void QFrameAllocator::clear()
{
    Q_D(QFrameAllocator);
    for (int i = 0, n = d->m_allocatorPool.size(); i < n; ++i)
        d->m_allocatorPool[i].clear();
}

// Trim excess memory used by chunks
void QFrameAllocator::trim()
{
    Q_D(QFrameAllocator);
    for (int i = 0, n = d->m_allocatorPool.size(); i < n; ++i)
        d->m_allocatorPool[i].trim();
}

uint QFrameAllocator::maxObjectSize() const
{
    Q_D(const QFrameAllocator);
    return d->m_maxObjectSize;
}

int QFrameAllocator::allocatorPoolSize() const
{
    Q_D(const QFrameAllocator);
    return d->m_allocatorPool.size();
}

bool QFrameAllocator::isEmpty() const
{
    Q_D(const QFrameAllocator);
    for (const QFixedFrameAllocator &allocator : d->m_allocatorPool) {
        if (!allocator.isEmpty())
            return false;
    }
    return true;
}

uint QFrameAllocator::totalChunkCount() const
{
    Q_D(const QFrameAllocator);
    uint chunkCount = 0;
    for (const QFixedFrameAllocator& allocator : d->m_allocatorPool)
        chunkCount += allocator.chunkCount();
    return chunkCount;
}

QFixedFrameAllocator::QFixedFrameAllocator()
    : m_blockSize(0)
    , m_nbrBlock(0)
    , m_lastAllocatedChunck(nullptr)
    , m_lastFreedChunck(nullptr)
{
}

QFixedFrameAllocator::~QFixedFrameAllocator()
{
    release();
}

void QFixedFrameAllocator::init(uint blockSize, uchar pageSize)
{
    m_blockSize = blockSize;
    m_nbrBlock = pageSize;
}

void *QFixedFrameAllocator::allocate()
{
    Q_ASSERT(m_blockSize);
    return scan().allocate(m_blockSize);
}

QFrameChunk &QFixedFrameAllocator::scan()
{
    Q_ASSERT(m_blockSize);
    Q_ASSERT(m_nbrBlock);

    if (m_lastAllocatedChunck && m_lastAllocatedChunck->m_blocksAvailable)
        return *m_lastAllocatedChunck;

    for (int i = 0; i < m_chunks.size(); i++) {
        if (m_chunks[i].m_blocksAvailable > 0) {
            m_lastAllocatedChunck = m_chunks.begin() + i;
            return *m_lastAllocatedChunck;
        }
    }
    m_chunks.resize(m_chunks.size() + 1);
    QFrameChunk &newChunk = m_chunks.last();
    newChunk.init(m_blockSize, m_nbrBlock);
    m_lastAllocatedChunck = &newChunk;
    m_lastFreedChunck = &newChunk;
    return newChunk;
}

void QFixedFrameAllocator::deallocate(void *ptr)
{
    Q_ASSERT(m_blockSize && m_nbrBlock);
    if (!m_chunks.empty() && ptr != nullptr) {
        if (m_lastFreedChunck != nullptr && m_lastFreedChunck->contains(ptr, m_blockSize))
            m_lastFreedChunck->deallocate(ptr, m_blockSize);
        else {
            for (int i = 0; i < m_chunks.size(); i++) {
                if (m_chunks[i].contains(ptr, m_blockSize)) {
                    m_chunks[i].deallocate(ptr, m_blockSize);
                    m_lastFreedChunck = m_chunks.begin() + i;
                    break ;
                }
            }
        }
    }
}

void QFixedFrameAllocator::trim()
{
    for (int i = m_chunks.size() - 1; i >= 0; i--) {
        if (m_chunks.at(i).isEmpty()) {
            m_chunks[i].release();
            if (m_lastAllocatedChunck == &m_chunks[i])
                m_lastAllocatedChunck = nullptr;
            if (m_lastFreedChunck == &m_chunks[i])
                m_lastFreedChunck = nullptr;
            m_chunks.removeAt(i);
        }
    }
}

void QFixedFrameAllocator::release()
{
    for (int i = m_chunks.size() - 1; i >= 0; i--)
        m_chunks[i].release();
    m_chunks.clear();
    m_lastAllocatedChunck = nullptr;
    m_lastFreedChunck = nullptr;
}

// Allows to reuse chunks without having to reinitialize and reallocate them
void QFixedFrameAllocator::clear()
{
    for (int i = m_chunks.size() - 1; i >= 0; i--)
        m_chunks[i].clear(m_blockSize, m_nbrBlock);
}

bool QFixedFrameAllocator::isEmpty() const
{
    for (const QFrameChunk &chunck : m_chunks) {
        if (chunck.m_blocksAvailable != chunck.m_maxBlocksAvailable)
            return false;
    }
    return true;
}

// QFrameChuck is agnostic about blocksize
// However if it was initialized with a block size of 16
// You should then pass 16 to allocate and deallocate
void QFrameChunk::init(uint blockSize, uchar blocks)
{
    m_data = new uchar[blockSize * blocks];
    m_firstAvailableBlock = 0;
    m_blocksAvailable = blocks;
    m_maxBlocksAvailable = blocks;
    uchar *p = m_data;
    // Init each block with its position stored in its first byte
    for (uchar i = 0; i < blocks; p += blockSize)
        *p = ++i;
#ifdef QFRAMEALLOCATOR_DEBUG
    VALGRIND_CREATE_MEMPOOL(m_data, 0, true);
    VALGRIND_MAKE_MEM_NOACCESS(m_data, blockSize * blocks);
    VALGRIND_MEMPOOL_ALLOC(m_data, m_data, blockSize * blocks);
#endif
}

void *QFrameChunk::allocate(uint blockSize)
{
    if (m_blocksAvailable == 0)
        return nullptr;
    uchar *r = m_data + (m_firstAvailableBlock * blockSize);
    m_firstAvailableBlock = *r;
    --m_blocksAvailable;
    return r;
}

// Shouldn't be called more than once for the same pointer
void QFrameChunk::deallocate(void *p, uint blockSize)
{
    if (p >= m_data) {
        uchar *toRelease = static_cast<uchar *>(p);
        uchar oldFreeBlock = m_firstAvailableBlock;
        m_firstAvailableBlock = static_cast<uchar>((toRelease - m_data) / blockSize);
        *toRelease = oldFreeBlock;
        ++m_blocksAvailable;
    }
}

bool QFrameChunk::contains(void *p, uint blockSize)
{
    uchar *c = static_cast<uchar *>(p);
    return (m_data <= c && c < m_data + blockSize * m_maxBlocksAvailable);
}

// Reset chunck without releasing heap allocated memory
void QFrameChunk::clear(uint blockSize, uchar blocks)
{
    m_firstAvailableBlock = 0;
    m_blocksAvailable = blocks;

    uchar *p = m_data;
    // Init each block with its position stored in its first byte
    for (uchar i = 0; i < blocks; p += blockSize)
        *p = ++i;
}

void QFrameChunk::release()
{
#ifdef QFRAMEALLOCATOR_DEBUG
    VALGRIND_MEMPOOL_FREE(m_data, m_data);
    VALGRIND_DESTROY_MEMPOOL(m_data);
#endif
    delete [] m_data;
}

void* QFrameAllocator::allocateRawMemory(size_t size)
{
    Q_D(QFrameAllocator);
    Q_ASSERT(size <= d->m_maxObjectSize);
    uint allocatorIndex = d->allocatorIndexFromSize(uint(size));
    return d->allocateAtChunk(allocatorIndex);
}

void QFrameAllocator::deallocateRawMemory(void* ptr, size_t size)
{
    Q_D(QFrameAllocator);
    Q_ASSERT(size <= d->m_maxObjectSize);
    uint allocatorIndex = d->allocatorIndexFromSize(uint(size));
    d->deallocateAtChunck(ptr, allocatorIndex);
}

} // Qt3D

QT_END_NAMESPACE
