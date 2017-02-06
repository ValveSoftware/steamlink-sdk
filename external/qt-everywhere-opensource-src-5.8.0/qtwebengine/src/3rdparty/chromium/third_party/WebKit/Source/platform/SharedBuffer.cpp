/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/SharedBuffer.h"

#include "platform/web_process_memory_dump.h"
#include "wtf/text/UTF8.h"
#include "wtf/text/Unicode.h"

#undef SHARED_BUFFER_STATS

#ifdef SHARED_BUFFER_STATS
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/DataLog.h"
#include <set>
#endif

namespace blink {

static inline size_t segmentIndex(size_t position)
{
    return position / SharedBuffer::kSegmentSize;
}

static inline size_t offsetInSegment(size_t position)
{
    return position % SharedBuffer::kSegmentSize;
}

static inline char* allocateSegment()
{
    return static_cast<char*>(WTF::Partitions::fastMalloc(SharedBuffer::kSegmentSize, "blink::SharedBuffer"));
}

static inline void freeSegment(char* p)
{
    WTF::Partitions::fastFree(p);
}

#ifdef SHARED_BUFFER_STATS

static Mutex& statsMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static std::set<SharedBuffer*>& liveBuffers()
{
    // Use std::set instead of WTF::HashSet to avoid increasing PartitionAlloc
    // memory usage.
    DEFINE_STATIC_LOCAL(std::set<SharedBuffer*>, buffers, ());
    return buffers;
}

static bool sizeComparator(SharedBuffer* a, SharedBuffer* b)
{
    return a->size() > b->size();
}

static CString snippetForBuffer(SharedBuffer* sharedBuffer)
{
    const size_t kMaxSnippetLength = 64;
    char* snippet = 0;
    size_t snippetLength = std::min(sharedBuffer->size(), kMaxSnippetLength);
    CString result = CString::newUninitialized(snippetLength, snippet);

    const char* segment;
    size_t offset = 0;
    while (size_t segmentLength = sharedBuffer->getSomeDataInternal(segment, offset)) {
        size_t length = std::min(segmentLength, snippetLength - offset);
        memcpy(snippet + offset, segment, length);
        offset += segmentLength;
        if (offset >= snippetLength)
            break;
    }

    for (size_t i = 0; i < snippetLength; ++i) {
        if (!isASCIIPrintable(snippet[i]))
            snippet[i] = '?';
    }

    return result;
}

static void printStats()
{
    MutexLocker locker(statsMutex());
    Vector<SharedBuffer*> buffers;
    for (auto* buffer : liveBuffers())
        buffers.append(buffer);
    std::sort(buffers.begin(), buffers.end(), sizeComparator);

    dataLogF("---- Shared Buffer Stats ----\n");
    for (size_t i = 0; i < buffers.size() && i < 64; ++i) {
        CString snippet = snippetForBuffer(buffers[i]);
        dataLogF("Buffer size=%8u %s\n", buffers[i]->size(), snippet.data());
    }
}

static void didCreateSharedBuffer(SharedBuffer* buffer)
{
    MutexLocker locker(statsMutex());
    liveBuffers().insert(buffer);

    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, WTF::bind(&printStats));
}

static void willDestroySharedBuffer(SharedBuffer* buffer)
{
    MutexLocker locker(statsMutex());
    liveBuffers().erase(buffer);
}

#endif

SharedBuffer::SharedBuffer()
    : m_size(0)
    , m_buffer(PurgeableVector::NotPurgeable)
{
#ifdef SHARED_BUFFER_STATS
    didCreateSharedBuffer(this);
#endif
}

SharedBuffer::SharedBuffer(size_t size)
    : m_size(size)
    , m_buffer(PurgeableVector::NotPurgeable)
{
    m_buffer.reserveCapacity(size);
    m_buffer.grow(size);
#ifdef SHARED_BUFFER_STATS
    didCreateSharedBuffer(this);
#endif
}

SharedBuffer::SharedBuffer(const char* data, size_t size)
    : m_size(0)
    , m_buffer(PurgeableVector::NotPurgeable)
{
    appendInternal(data, size);

#ifdef SHARED_BUFFER_STATS
    didCreateSharedBuffer(this);
#endif
}

SharedBuffer::SharedBuffer(const char* data, size_t size, PurgeableVector::PurgeableOption purgeable)
    : m_size(0)
    , m_buffer(purgeable)
{
    appendInternal(data, size);

#ifdef SHARED_BUFFER_STATS
    didCreateSharedBuffer(this);
#endif
}

SharedBuffer::SharedBuffer(const unsigned char* data, size_t size)
    : m_size(0)
    , m_buffer(PurgeableVector::NotPurgeable)
{
    appendInternal(reinterpret_cast<const char*>(data), size);

#ifdef SHARED_BUFFER_STATS
    didCreateSharedBuffer(this);
#endif
}

SharedBuffer::~SharedBuffer()
{
    clear();

#ifdef SHARED_BUFFER_STATS
    willDestroySharedBuffer(this);
#endif
}

PassRefPtr<SharedBuffer> SharedBuffer::adoptVector(Vector<char>& vector)
{
    RefPtr<SharedBuffer> buffer = create();
    buffer->m_buffer.adopt(vector);
    buffer->m_size = buffer->m_buffer.size();
    return buffer.release();
}

size_t SharedBuffer::size() const
{
    return m_size;
}

const char* SharedBuffer::data() const
{
    mergeSegmentsIntoBuffer();
    return m_buffer.data();
}

void SharedBuffer::append(PassRefPtr<SharedBuffer> data)
{
    const char* segment;
    size_t position = 0;
    while (size_t length = data->getSomeDataInternal(segment, position)) {
        append(segment, length);
        position += length;
    }
}

void SharedBuffer::appendInternal(const char* data, size_t length)
{
    ASSERT(isLocked());
    if (!length)
        return;

    ASSERT(m_size >= m_buffer.size());
    size_t positionInSegment = offsetInSegment(m_size - m_buffer.size());
    m_size += length;

    if (m_size <= kSegmentSize) {
        // No need to use segments for small resource data.
        m_buffer.append(data, length);
        return;
    }

    char* segment;
    if (!positionInSegment) {
        segment = allocateSegment();
        m_segments.append(segment);
    } else
        segment = m_segments.last() + positionInSegment;

    size_t segmentFreeSpace = kSegmentSize - positionInSegment;
    size_t bytesToCopy = std::min(length, segmentFreeSpace);

    for (;;) {
        memcpy(segment, data, bytesToCopy);
        if (length == bytesToCopy)
            break;

        length -= bytesToCopy;
        data += bytesToCopy;
        segment = allocateSegment();
        m_segments.append(segment);
        bytesToCopy = std::min(length, static_cast<size_t>(kSegmentSize));
    }
}

void SharedBuffer::append(const Vector<char>& data)
{
    append(data.data(), data.size());
}

void SharedBuffer::clear()
{
    for (size_t i = 0; i < m_segments.size(); ++i)
        freeSegment(m_segments[i]);

    m_segments.clear();
    m_size = 0;
    m_buffer.clear();
}

PassRefPtr<SharedBuffer> SharedBuffer::copy() const
{
    RefPtr<SharedBuffer> clone(adoptRef(new SharedBuffer));
    clone->m_size = m_size;
    clone->m_buffer.reserveCapacity(m_size);
    clone->m_buffer.append(m_buffer.data(), m_buffer.size());
    if (!m_segments.isEmpty()) {
        const char* segment = 0;
        size_t position = m_buffer.size();
        while (size_t segmentSize = getSomeDataInternal(segment, position)) {
            clone->m_buffer.append(segment, segmentSize);
            position += segmentSize;
        }
        ASSERT(position == clone->size());
    }
    return clone.release();
}

void SharedBuffer::mergeSegmentsIntoBuffer() const
{
    size_t bufferSize = m_buffer.size();
    if (m_size > bufferSize) {
        size_t bytesLeft = m_size - bufferSize;
        for (size_t i = 0; i < m_segments.size(); ++i) {
            size_t bytesToCopy = std::min(bytesLeft, static_cast<size_t>(kSegmentSize));
            m_buffer.append(m_segments[i], bytesToCopy);
            bytesLeft -= bytesToCopy;
            freeSegment(m_segments[i]);
        }
        m_segments.clear();
    }
}

size_t SharedBuffer::getSomeDataInternal(const char*& someData, size_t position) const
{
    ASSERT(isLocked());
    size_t totalSize = size();
    if (position >= totalSize) {
        someData = 0;
        return 0;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(position < m_size);
    size_t consecutiveSize = m_buffer.size();
    if (position < consecutiveSize) {
        someData = m_buffer.data() + position;
        return consecutiveSize - position;
    }

    position -= consecutiveSize;
    size_t segments = m_segments.size();
    size_t maxSegmentedSize = segments * kSegmentSize;
    size_t segment = segmentIndex(position);
    if (segment < segments) {
        size_t bytesLeft = totalSize - consecutiveSize;
        size_t segmentedSize = std::min(maxSegmentedSize, bytesLeft);

        size_t positionInSegment = offsetInSegment(position);
        someData = m_segments[segment] + positionInSegment;
        return segment == segments - 1 ? segmentedSize - position : kSegmentSize - positionInSegment;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool SharedBuffer::getAsBytesInternal(void* dest, size_t byteLength) const
{
    if (!dest || byteLength != size())
        return false;

    const char* segment = 0;
    size_t position = 0;
    while (size_t segmentSize = getSomeDataInternal(segment, position)) {
        memcpy(static_cast<char*>(dest) + position, segment, segmentSize);
        position += segmentSize;
    }

    if (position != byteLength) {
        ASSERT_NOT_REACHED();
        // Don't return the incomplete data.
        return false;
    }

    return true;
}

PassRefPtr<SkData> SharedBuffer::getAsSkData() const
{
    size_t bufferLength = size();
    SkData* data = SkData::NewUninitialized(bufferLength);
    char* buffer = static_cast<char*>(data->writable_data());
    const char* segment = 0;
    size_t position = 0;
    while (size_t segmentSize = getSomeDataInternal(segment, position)) {
        memcpy(buffer + position, segment, segmentSize);
        position += segmentSize;
    }

    if (position != bufferLength) {
        ASSERT_NOT_REACHED();
        // Don't return the incomplete SkData.
        return nullptr;
    }
    return adoptRef(data);
}

bool SharedBuffer::lock()
{
    return m_buffer.lock();
}

void SharedBuffer::unlock()
{
    mergeSegmentsIntoBuffer();
    m_buffer.unlock();
}

bool SharedBuffer::isLocked() const
{
    return m_buffer.isLocked();
}

void SharedBuffer::onMemoryDump(const String& dumpPrefix, WebProcessMemoryDump* memoryDump) const
{
    if (m_buffer.size()) {
        m_buffer.onMemoryDump(dumpPrefix + "/shared_buffer", memoryDump);
    } else {
        // If there is data in the segments, then it should have been allocated
        // using fastMalloc.
        const String dataDumpName = dumpPrefix + "/segments";
        auto dump = memoryDump->createMemoryAllocatorDump(dataDumpName);
        dump->addScalar("size", "bytes", m_size);
        memoryDump->addSuballocation(dump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
    }
}

} // namespace blink
