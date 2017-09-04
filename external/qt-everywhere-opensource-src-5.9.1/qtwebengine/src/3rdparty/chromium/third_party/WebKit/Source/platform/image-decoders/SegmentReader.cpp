// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/SegmentReader.h"

#include "platform/SharedBuffer.h"
#include "third_party/skia/include/core/SkData.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

// SharedBufferSegmentReader ---------------------------------------------------

// Interface for ImageDecoder to read a SharedBuffer.
class SharedBufferSegmentReader final : public SegmentReader {
  WTF_MAKE_NONCOPYABLE(SharedBufferSegmentReader);

 public:
  SharedBufferSegmentReader(PassRefPtr<SharedBuffer>);
  size_t size() const override;
  size_t getSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> getAsSkData() const override;

 private:
  RefPtr<SharedBuffer> m_sharedBuffer;
};

SharedBufferSegmentReader::SharedBufferSegmentReader(
    PassRefPtr<SharedBuffer> buffer)
    : m_sharedBuffer(buffer) {}

size_t SharedBufferSegmentReader::size() const {
  return m_sharedBuffer->size();
}

size_t SharedBufferSegmentReader::getSomeData(const char*& data,
                                              size_t position) const {
  return m_sharedBuffer->getSomeData(data, position);
}

sk_sp<SkData> SharedBufferSegmentReader::getAsSkData() const {
  return m_sharedBuffer->getAsSkData();
}

// DataSegmentReader -----------------------------------------------------------

// Interface for ImageDecoder to read an SkData.
class DataSegmentReader final : public SegmentReader {
  WTF_MAKE_NONCOPYABLE(DataSegmentReader);

 public:
  DataSegmentReader(sk_sp<SkData>);
  size_t size() const override;
  size_t getSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> getAsSkData() const override;

 private:
  sk_sp<SkData> m_data;
};

DataSegmentReader::DataSegmentReader(sk_sp<SkData> data)
    : m_data(std::move(data)) {}

size_t DataSegmentReader::size() const {
  return m_data->size();
}

size_t DataSegmentReader::getSomeData(const char*& data,
                                      size_t position) const {
  if (position >= m_data->size())
    return 0;

  data = reinterpret_cast<const char*>(m_data->bytes() + position);
  return m_data->size() - position;
}

sk_sp<SkData> DataSegmentReader::getAsSkData() const {
  return m_data;
}

// ROBufferSegmentReader -------------------------------------------------------

class ROBufferSegmentReader final : public SegmentReader {
  WTF_MAKE_NONCOPYABLE(ROBufferSegmentReader);

 public:
  ROBufferSegmentReader(sk_sp<SkROBuffer>);

  size_t size() const override;
  size_t getSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> getAsSkData() const override;

 private:
  sk_sp<SkROBuffer> m_roBuffer;
  // Protects access to mutable fields.
  mutable Mutex m_readMutex;
  // Position of the first char in the current block of m_iter.
  mutable size_t m_positionOfBlock;
  mutable SkROBuffer::Iter m_iter;
};

ROBufferSegmentReader::ROBufferSegmentReader(sk_sp<SkROBuffer> buffer)
    : m_roBuffer(std::move(buffer)),
      m_positionOfBlock(0),
      m_iter(m_roBuffer.get()) {}

size_t ROBufferSegmentReader::size() const {
  return m_roBuffer ? m_roBuffer->size() : 0;
}

size_t ROBufferSegmentReader::getSomeData(const char*& data,
                                          size_t position) const {
  if (!m_roBuffer)
    return 0;

  MutexLocker lock(m_readMutex);

  if (position < m_positionOfBlock) {
    // SkROBuffer::Iter only iterates forwards. Start from the beginning.
    m_iter.reset(m_roBuffer.get());
    m_positionOfBlock = 0;
  }

  for (size_t sizeOfBlock = m_iter.size(); sizeOfBlock != 0;
       m_positionOfBlock += sizeOfBlock, sizeOfBlock = m_iter.size()) {
    ASSERT(m_positionOfBlock <= position);

    if (m_positionOfBlock + sizeOfBlock > position) {
      // |position| is in this block.
      const size_t positionInBlock = position - m_positionOfBlock;
      data = static_cast<const char*>(m_iter.data()) + positionInBlock;
      return sizeOfBlock - positionInBlock;
    }

    // Move to next block.
    if (!m_iter.next()) {
      // Reset to the beginning, so future calls can succeed.
      m_iter.reset(m_roBuffer.get());
      m_positionOfBlock = 0;
      return 0;
    }
  }

  return 0;
}

static void unrefROBuffer(const void* ptr, void* context) {
  static_cast<SkROBuffer*>(context)->unref();
}

sk_sp<SkData> ROBufferSegmentReader::getAsSkData() const {
  if (!m_roBuffer)
    return nullptr;

  // Check to see if the data is already contiguous.
  SkROBuffer::Iter iter(m_roBuffer.get());
  const bool multipleBlocks = iter.next();
  iter.reset(m_roBuffer.get());

  if (!multipleBlocks) {
    // Contiguous data. No need to copy.
    m_roBuffer->ref();
    return SkData::MakeWithProc(iter.data(), iter.size(), &unrefROBuffer,
                                m_roBuffer.get());
  }

  sk_sp<SkData> data = SkData::MakeUninitialized(m_roBuffer->size());
  char* dst = static_cast<char*>(data->writable_data());
  do {
    size_t size = iter.size();
    memcpy(dst, iter.data(), size);
    dst += size;
  } while (iter.next());
  return data;
}

// SegmentReader ---------------------------------------------------------------

PassRefPtr<SegmentReader> SegmentReader::createFromSharedBuffer(
    PassRefPtr<SharedBuffer> buffer) {
  return adoptRef(new SharedBufferSegmentReader(std::move(buffer)));
}

PassRefPtr<SegmentReader> SegmentReader::createFromSkData(sk_sp<SkData> data) {
  return adoptRef(new DataSegmentReader(std::move(data)));
}

PassRefPtr<SegmentReader> SegmentReader::createFromSkROBuffer(
    sk_sp<SkROBuffer> buffer) {
  return adoptRef(new ROBufferSegmentReader(std::move(buffer)));
}

}  // namespace blink
