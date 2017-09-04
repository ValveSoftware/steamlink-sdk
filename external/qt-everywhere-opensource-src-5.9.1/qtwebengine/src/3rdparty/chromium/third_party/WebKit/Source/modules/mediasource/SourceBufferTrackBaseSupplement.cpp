// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasource/SourceBufferTrackBaseSupplement.h"

#include "core/html/track/TrackBase.h"
#include "modules/mediasource/SourceBuffer.h"

namespace blink {

static const char* kSupplementName = "SourceBufferTrackBaseSupplement";

// static
SourceBufferTrackBaseSupplement* SourceBufferTrackBaseSupplement::fromIfExists(
    TrackBase& track) {
  return static_cast<SourceBufferTrackBaseSupplement*>(
      Supplement<TrackBase>::from(track, kSupplementName));
}

// static
SourceBufferTrackBaseSupplement& SourceBufferTrackBaseSupplement::from(
    TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = fromIfExists(track);
  if (!supplement) {
    supplement = new SourceBufferTrackBaseSupplement();
    Supplement<TrackBase>::provideTo(track, kSupplementName, supplement);
  }
  return *supplement;
}

// static
SourceBuffer* SourceBufferTrackBaseSupplement::sourceBuffer(TrackBase& track) {
  SourceBufferTrackBaseSupplement* supplement = fromIfExists(track);
  if (supplement)
    return supplement->m_sourceBuffer;
  return nullptr;
}

void SourceBufferTrackBaseSupplement::setSourceBuffer(
    TrackBase& track,
    SourceBuffer* sourceBuffer) {
  from(track).m_sourceBuffer = sourceBuffer;
}

DEFINE_TRACE(SourceBufferTrackBaseSupplement) {
  visitor->trace(m_sourceBuffer);
  Supplement<TrackBase>::trace(visitor);
}

}  // namespace blink
