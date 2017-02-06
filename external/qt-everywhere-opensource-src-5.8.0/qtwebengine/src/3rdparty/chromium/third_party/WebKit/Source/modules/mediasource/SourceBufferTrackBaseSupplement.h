// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SourceBufferTrackBaseSupplement_h
#define SourceBufferTrackBaseSupplement_h

#include "platform/Supplementable.h"
#include "wtf/Allocator.h"

namespace blink {

class TrackBase;
class SourceBuffer;

class SourceBufferTrackBaseSupplement : public GarbageCollected<SourceBufferTrackBaseSupplement>, public Supplement<TrackBase> {
    USING_GARBAGE_COLLECTED_MIXIN(SourceBufferTrackBaseSupplement);
public:
    static SourceBuffer* sourceBuffer(TrackBase&);
    static void setSourceBuffer(TrackBase&, SourceBuffer*);

    DECLARE_VIRTUAL_TRACE();

private:
    static SourceBufferTrackBaseSupplement& from(TrackBase&);
    static SourceBufferTrackBaseSupplement* fromIfExists(TrackBase&);

    Member<SourceBuffer> m_sourceBuffer;
};

} // namespace blink

#endif

