// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SegmentReader_h
#define SegmentReader_h

#include "platform/SharedBuffer.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRWBuffer.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace blink {

// Interface that looks like SharedBuffer. Used by ImageDecoders to use various
// sources of input including:
// - SharedBuffer
//   - for when the caller already has a SharedBuffer
// - SkData
//   - for when the caller already has an SkData
// - SkROBuffer
//   - for when the caller wants to read/write in different threads
//
// Unlike SharedBuffer, this is a read-only interface. There is no way to
// modify the underlying data source.
class PLATFORM_EXPORT SegmentReader : public ThreadSafeRefCounted<SegmentReader> {
    WTF_MAKE_NONCOPYABLE(SegmentReader);
public:
    // This version is thread-safe so long as no thread is modifying the
    // underlying SharedBuffer. This class does not modify it, so that would
    // mean modifying it in another way.
    static PassRefPtr<SegmentReader> createFromSharedBuffer(PassRefPtr<SharedBuffer>);

    // These versions use thread-safe input, so they are always thread-safe.
    static PassRefPtr<SegmentReader> createFromSkData(PassRefPtr<SkData>);
    static PassRefPtr<SegmentReader> createFromSkROBuffer(PassRefPtr<SkROBuffer>);

    SegmentReader() {}
    virtual ~SegmentReader() {}
    virtual size_t size() const = 0;
    virtual size_t getSomeData(const char*& data, size_t position) const = 0;
    virtual PassRefPtr<SkData> getAsSkData() const = 0;
};

} // namespace blink
#endif // SegmentReader_h
