/*
 * Copyright (C) 2015 The Qt Company Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if OS(SYMBIAN)

#include "RegisterFileAllocatorSymbian.h"

namespace WTF {

/** Efficiently allocates memory pools of size poolSize.
 * Primarily designed for JSC RegisterFile's needs.
 * Not thread-safe.
 */
RegisterFileAllocator::RegisterFileAllocator(TUint32 reservationSize, TUint32 poolSize) :
    m_reserved(reservationSize), m_poolSize(poolSize)
{
    // Get system's page size value.
    SYMBIAN_PAGESIZE(m_pageSize);

    // We only accept multiples of system page size for both initial reservation
    // and the alignment/pool size
    m_reserved = SYMBIAN_ROUNDUPTOMULTIPLE(m_reserved, m_pageSize);
    __ASSERT_ALWAYS(SYMBIAN_ROUNDUPTOMULTIPLE(m_poolSize, m_pageSize),
                    User::Panic(_L("RegisterFileAllocator1"), KErrArgument));

    // Open a Symbian RChunk, and reserve requested virtual address range
    // Any thread in this process can operate this RChunk due to EOwnerProcess access rights.
    TInt ret = m_chunk.CreateDisconnectedLocal(0 , 0, (TInt)m_reserved , EOwnerProcess);
    if (ret != KErrNone)
        User::Panic(_L("RegisterFileAllocator2"), ret);

    m_buffer = (void*)m_chunk.Base();
    m_resEnd = (void*)(m_chunk.Base() + m_chunk.MaxSize());
    m_comEnd = m_buffer;
}

RegisterFileAllocator::~RegisterFileAllocator()
{
    // release everything!
    m_chunk.Decommit(0, m_chunk.MaxSize());
    m_chunk.Close();
}

void* RegisterFileAllocator::buffer() const
{
    return m_buffer;
}

void RegisterFileAllocator::grow(void* newEnd)
{
    // trying to commit more memory than reserved!
    if (newEnd > m_resEnd)
        return;

    if (newEnd > m_comEnd) {
        TInt nBytes = (TInt)(newEnd) - (TInt)(m_comEnd);
        nBytes = SYMBIAN_ROUNDUPTOMULTIPLE(nBytes, m_poolSize);
        TInt offset = (TInt)m_comEnd - (TInt)m_buffer;
        // The reserved size is not guaranteed to be a multiple of the pool size.
        TInt maxBytes = (TInt)m_resEnd - (TInt)m_comEnd;
        if (nBytes > maxBytes)
            nBytes = maxBytes;

        TInt ret = m_chunk.Commit(offset, nBytes);
        if (ret == KErrNone)
            m_comEnd = (void*)(m_chunk.Base() + m_chunk.Size());
        else
            CRASH();
    }
}

void RegisterFileAllocator::shrink(void* newEnd)
{
    if (newEnd < m_comEnd) {
        TInt nBytes = (TInt)newEnd - (TInt)m_comEnd;
        if (nBytes >= m_poolSize) {
            TInt offset = SYMBIAN_ROUNDUPTOMULTIPLE((TUint)newEnd, m_poolSize) - (TInt)m_buffer;
            nBytes = (TInt)m_comEnd - offset - (TInt)m_buffer;
            if (nBytes > 0) {
                TInt ret = m_chunk.Decommit(offset, nBytes);
                if (ret == KErrNone)
                    m_comEnd = (void*)(m_chunk.Base() + m_chunk.Size());
            }
        }
    }
}

} // end of namespace

#endif // SYMBIAN
