// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_SEQUENCE_H_
#define CC_SURFACES_SURFACE_SEQUENCE_H_

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "base/hash.h"
#include "cc/surfaces/frame_sink_id.h"

namespace cc {

// A per-surface-namespace sequence number that's used to coordinate
// dependencies between frames. A sequence number may be satisfied once, and
// may be depended on once.
struct SurfaceSequence {
  SurfaceSequence() : sequence(0u) {}
  SurfaceSequence(const FrameSinkId& frame_sink_id, uint32_t sequence)
      : frame_sink_id(frame_sink_id), sequence(sequence) {}
  bool is_valid() const { return frame_sink_id.is_valid() && sequence > 0u; }

  FrameSinkId frame_sink_id;
  uint32_t sequence;
};

inline bool operator==(const SurfaceSequence& a, const SurfaceSequence& b) {
  return a.frame_sink_id == b.frame_sink_id && a.sequence == b.sequence;
}

inline bool operator!=(const SurfaceSequence& a, const SurfaceSequence& b) {
  return !(a == b);
}

inline bool operator<(const SurfaceSequence& a, const SurfaceSequence& b) {
  return std::tie(a.frame_sink_id, a.sequence) <
         std::tie(b.frame_sink_id, b.sequence);
}

struct SurfaceSequenceHash {
  size_t operator()(SurfaceSequence key) const {
    return base::HashInts(static_cast<uint64_t>(key.frame_sink_id.hash()),
                          key.sequence);
  }
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_SEQUENCE_H_
