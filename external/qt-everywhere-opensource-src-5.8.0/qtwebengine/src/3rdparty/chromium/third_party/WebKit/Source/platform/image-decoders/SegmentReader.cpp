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
    PassRefPtr<SkData> getAsSkData() const override;
private:
    RefPtr<SharedBuffer> m_sharedBuffer;
};

SharedBufferSegmentReader::SharedBufferSegmentReader(PassRefPtr<SharedBuffer> buffer)
    : m_sharedBuffer(buffer) {}

size_t SharedBufferSegmentReader::size() const
{
    return m_sharedBuffer->size();
}

size_t SharedBufferSegmentReader::getSomeData(const char*& data, size_t position) const
{
    return m_sharedBuffer->getSomeData(data, position);
}

PassRefPtr<SkData> SharedBufferSegmentReader::getAsSkData() const
{
    return m_sharedBuffer->getAsSkData();
}

// DataSegmentReader -----------------------------------------------------------

// Interface for ImageDecoder to read an SkData.
class DataSegmentReader final : public SegmentReader {
    WTF_MAKE_NONCOPYABLE(DataSegmentReader);
public:
    DataSegmentReader(PassRefPtr<SkData>);
    size_t size() const override;
    size_t getSomeData(const char*& data, size_t position) const override;
    PassRefPtr<SkData> getAsSkData() const override;
private:
    RefPtr<SkData> m_data;
};

DataSegmentReader::DataSegmentReader(PassRefPtr<SkData> data)
    : m_data(data) {}

size_t DataSegmentReader::size() const
{
    return m_data->size();
}

size_t DataSegmentReader::getSomeData(const char*& data, size_t position) const
{
    if (position >= m_data->size())
        return 0;

    data = reinterpret_cast<const char*>(m_data->bytes() + position);
    return m_data->size() - position;
}

PassRefPtr<SkData> DataSegmentReader::getAsSkData() const
{
    return m_data.get();
}

// ROBufferSegmentReader -------------------------------------------------------

class ROBufferSegmentReader final : public SegmentReader {
    WTF_MAKE_NONCOPYABLE(ROBufferSegmentReader);
public:
    ROBufferSegmentReader(PassRefPtr<SkROBuffer>);

    size_t size() const override;
    size_t getSomeData(const char*& data, size_t position) const override;
    PassRefPtr<SkData> getAsSkData() const override;

private:
    RefPtr<SkROBuffer> m_roBuffer;
    // Protects access to mutable fields.
    mutable Mutex m_readMutex;
    // Position of the first char in the current block of m_iter.
    mutable size_t m_positionOfBlock;
    mutable SkROBuffer::Iter m_iter;
};

ROBufferSegmentReader::ROBufferSegmentReader(PassRefPtr<SkROBuffer> buffer)
    : m_roBuffer(buffer)
    , m_positionOfBlock(0)
    , m_iter(m_roBuffer.get())
    {}

size_t ROBufferSegmentReader::size() const
{
    return m_roBuffer ? m_roBuffer->size() : 0;
}

size_t ROBufferSegmentReader::getSomeData(const char*& data, size_t position) const
{
    if (!m_roBuffer)
        return 0;

    MutexLocker lock(m_readMutex);

    if (position < m_positionOfBlock) {
        // SkROBuffer::Iter only iterates forwards. Start from the beginning.
        m_iter.reset(m_roBuffer.get());
        m_positionOfBlock = 0;
    }

    for (size_t sizeOfBlock = m_iter.size(); sizeOfBlock != 0; m_positionOfBlock += sizeOfBlock, sizeOfBlock = m_iter.size()) {
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

static void unrefROBuffer(const void* ptr, void* context)
{
    static_cast<SkROBuffer*>(context)->unref();
}

PassRefPtr<SkData> ROBufferSegmentReader::getAsSkData() const
{
    if (!m_roBuffer)
        return nullptr;

    // Check to see if the data is already contiguous.
    SkROBuffer::Iter iter(m_roBuffer.get());
    const bool multipleBlocks = iter.next();
    iter.reset(m_roBuffer.get());

    if (!multipleBlocks) {
        // Contiguous data. No need to copy.
        m_roBuffer->ref();
        return adoptRef(SkData::NewWithProc(iter.data(), iter.size(), &unrefROBuffer, m_roBuffer.get()));
    }

    RefPtr<SkData> data = adoptRef(SkData::NewUninitialized(m_roBuffer->size()));
    char* dst = static_cast<char*>(data->writable_data());
    do {
        size_t size = iter.size();
        memcpy(dst, iter.data(), size);
        dst += size;
    } while (iter.next());
    return data.release();
}

// SegmentReader ---------------------------------------------------------------

PassRefPtr<SegmentReader> SegmentReader::createFromSharedBuffer(PassRefPtr<SharedBuffer> buffer)
{
    return adoptRef(new SharedBufferSegmentReader(buffer));
}

PassRefPtr<SegmentReader> SegmentReader::createFromSkData(PassRefPtr<SkData> data)
{
    return adoptRef(new DataSegmentReader(data));
}

PassRefPtr<SegmentReader> SegmentReader::createFromSkROBuffer(PassRefPtr<SkROBuffer> buffer)
{
    return adoptRef(new ROBufferSegmentReader(buffer));
}

} // namespace blink

