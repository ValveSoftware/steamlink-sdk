/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FastSharedBufferReader_h
#define FastSharedBufferReader_h

#include "platform/PlatformExport.h"
#include "platform/image-decoders/SegmentReader.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

// This class is used by image decoders to avoid memory consolidation and
// therefore minimizes the cost of memory copying when the decoders
// repeatedly read from a buffer that is continually growing due to network
// traffic.
class PLATFORM_EXPORT FastSharedBufferReader final {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(FastSharedBufferReader);
public:
    FastSharedBufferReader(PassRefPtr<SegmentReader> data);

    void setData(PassRefPtr<SegmentReader>);

    // Returns a consecutive buffer that carries the data starting
    // at |dataPosition| with |length| bytes.
    // This method returns a pointer to a memory segment stored in
    // |m_data| if such a consecutive buffer can be found.
    // Otherwise copies into |buffer| and returns it.
    // Caller must ensure there are enough bytes in |m_data| and |buffer|.
    const char* getConsecutiveData(size_t dataPosition, size_t length, char* buffer) const;

    // Wraps SegmentReader::getSomeData().
    size_t getSomeData(const char*& someData, size_t dataPosition) const;

    // Returns a byte at |dataPosition|.
    // Caller must ensure there are enough bytes in |m_data|.
    inline char getOneByte(size_t dataPosition) const
    {
        return *getConsecutiveData(dataPosition, 1, 0);
    }

    size_t size() const
    {
        return m_data->size();
    }

    // This class caches the last access for faster subsequent reads. This
    // method clears that cache in case the SegmentReader has been modified
    // (e.g. with mergeSegmentsIntoBuffer on a wrapped SharedBuffer).
    void clearCache();

private:
    void getSomeDataInternal(size_t dataPosition) const;

    RefPtr<SegmentReader> m_data;

    // Caches the last segment of |m_data| accessed, since subsequent reads are
    // likely to re-access it.
    mutable const char* m_segment;
    mutable size_t m_segmentLength;

    // Data position in |m_data| pointed to by |m_segment|.
    mutable size_t m_dataPosition;
};

} // namespace blink

#endif
