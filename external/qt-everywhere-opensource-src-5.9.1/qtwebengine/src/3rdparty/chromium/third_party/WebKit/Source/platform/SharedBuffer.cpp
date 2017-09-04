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

#include "platform/tracing/web_process_memory_dump.h"
#include "wtf/text/UTF8.h"
#include "wtf/text/Unicode.h"

namespace blink {

static inline size_t segmentIndex(size_t position) {
  return position / SharedBuffer::kSegmentSize;
}

static inline size_t offsetInSegment(size_t position) {
  return position % SharedBuffer::kSegmentSize;
}

static inline char* allocateSegment() {
  return static_cast<char*>(WTF::Partitions::fastMalloc(
      SharedBuffer::kSegmentSize, "blink::SharedBuffer"));
}

static inline void freeSegment(char* p) {
  WTF::Partitions::fastFree(p);
}

SharedBuffer::SharedBuffer() : m_size(0) {}

SharedBuffer::SharedBuffer(size_t size) : m_size(size), m_buffer(size) {}

SharedBuffer::SharedBuffer(const char* data, size_t size) : m_size(0) {
  appendInternal(data, size);
}

SharedBuffer::SharedBuffer(const unsigned char* data, size_t size) : m_size(0) {
  appendInternal(reinterpret_cast<const char*>(data), size);
}

SharedBuffer::~SharedBuffer() {
  clear();
}

PassRefPtr<SharedBuffer> SharedBuffer::adoptVector(Vector<char>& vector) {
  RefPtr<SharedBuffer> buffer = create();
  buffer->m_buffer.swap(vector);
  buffer->m_size = buffer->m_buffer.size();
  return buffer.release();
}

size_t SharedBuffer::size() const {
  return m_size;
}

const char* SharedBuffer::data() const {
  mergeSegmentsIntoBuffer();
  return m_buffer.data();
}

void SharedBuffer::append(PassRefPtr<SharedBuffer> data) {
  const char* segment;
  size_t position = 0;
  while (size_t length = data->getSomeDataInternal(segment, position)) {
    append(segment, length);
    position += length;
  }
}

void SharedBuffer::appendInternal(const char* data, size_t length) {
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

void SharedBuffer::append(const Vector<char>& data) {
  append(data.data(), data.size());
}

void SharedBuffer::clear() {
  for (size_t i = 0; i < m_segments.size(); ++i)
    freeSegment(m_segments[i]);

  m_segments.clear();
  m_size = 0;
  m_buffer.clear();
}

PassRefPtr<SharedBuffer> SharedBuffer::copy() const {
  RefPtr<SharedBuffer> clone(adoptRef(new SharedBuffer));
  clone->m_size = m_size;
  clone->m_buffer.reserveInitialCapacity(m_size);
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

void SharedBuffer::mergeSegmentsIntoBuffer() const {
  size_t bufferSize = m_buffer.size();
  if (m_size > bufferSize) {
    size_t bytesLeft = m_size - bufferSize;
    for (size_t i = 0; i < m_segments.size(); ++i) {
      size_t bytesToCopy =
          std::min(bytesLeft, static_cast<size_t>(kSegmentSize));
      m_buffer.append(m_segments[i], bytesToCopy);
      bytesLeft -= bytesToCopy;
      freeSegment(m_segments[i]);
    }
    m_segments.clear();
  }
}

size_t SharedBuffer::getSomeDataInternal(const char*& someData,
                                         size_t position) const {
  size_t totalSize = size();
  if (position >= totalSize) {
    someData = 0;
    return 0;
  }

  SECURITY_DCHECK(position < m_size);
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
    return segment == segments - 1 ? segmentedSize - position
                                   : kSegmentSize - positionInSegment;
  }
  ASSERT_NOT_REACHED();
  return 0;
}

bool SharedBuffer::getAsBytesInternal(void* dest,
                                      size_t loadPosition,
                                      size_t byteLength) const {
  if (!dest)
    return false;

  const char* segment = nullptr;
  size_t writePosition = 0;
  while (byteLength > 0) {
    size_t loadSize = getSomeDataInternal(segment, loadPosition);
    if (loadSize == 0)
      break;

    if (byteLength < loadSize)
      loadSize = byteLength;
    memcpy(static_cast<char*>(dest) + writePosition, segment, loadSize);
    loadPosition += loadSize;
    writePosition += loadSize;
    byteLength -= loadSize;
  }

  return byteLength == 0;
}

sk_sp<SkData> SharedBuffer::getAsSkData() const {
  size_t bufferLength = size();
  sk_sp<SkData> data = SkData::MakeUninitialized(bufferLength);
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
  return data;
}

void SharedBuffer::onMemoryDump(const String& dumpPrefix,
                                WebProcessMemoryDump* memoryDump) const {
  if (m_buffer.size()) {
    WebMemoryAllocatorDump* dump =
        memoryDump->createMemoryAllocatorDump(dumpPrefix + "/shared_buffer");
    dump->addScalar("size", "bytes", m_buffer.size());
    memoryDump->addSuballocation(
        dump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
  } else {
    // If there is data in the segments, then it should have been allocated
    // using fastMalloc.
    const String dataDumpName = dumpPrefix + "/segments";
    auto dump = memoryDump->createMemoryAllocatorDump(dataDumpName);
    dump->addScalar("size", "bytes", m_size);
    memoryDump->addSuballocation(
        dump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
  }
}

}  // namespace blink
