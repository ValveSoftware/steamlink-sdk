// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunk_h
#define PaintChunk_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "wtf/Allocator.h"
#include "wtf/Optional.h"
#include "wtf/Vector.h"
#include <iosfwd>

namespace blink {

// A contiguous sequence of drawings with common paint properties.
//
// This is expected to be owned by the paint artifact which also owns the
// related drawings.
//
// This is a Slimming Paint v2 class.
struct PaintChunk {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  PaintChunk() : beginIndex(0), endIndex(0), knownToBeOpaque(false) {}
  PaintChunk(size_t begin,
             size_t end,
             const DisplayItem::Id* chunkId,
             const PaintChunkProperties& props)
      : beginIndex(begin),
        endIndex(end),
        properties(props),
        knownToBeOpaque(false) {
    if (chunkId)
      id.emplace(*chunkId);
  }

  size_t size() const {
    ASSERT(endIndex >= beginIndex);
    return endIndex - beginIndex;
  }

  // Check if a new PaintChunk (this) created in the latest paint matches an old
  // PaintChunk created in the previous paint.
  bool matches(const PaintChunk& old) const {
    // A PaintChunk without an id doesn't match any other PaintChunks.
    if (!id || !old.id)
      return false;
    if (*id != *old.id)
      return false;
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    CHECK(id->client.isAlive());
#endif
    // A chunk whose client is just created should not match any cached chunk,
    // even if it's id equals the old chunk's id (which may happen if this
    // chunk's client is just created at the same address of the old chunk's
    // deleted client).
    return !id->client.isJustCreated();
  }

  // Index of the first drawing in this chunk.
  size_t beginIndex;

  // Index of the first drawing not in this chunk, so that there are
  // |endIndex - beginIndex| drawings in the chunk.
  size_t endIndex;

  // Identifier of this chunk. If it has a value, it should be unique. This is
  // used to match a new chunk to a cached old chunk to track changes of chunk
  // contents, so the id should be stable across document cycles. If the
  // contents of the chunk can't be cached (e.g. it's created when
  // PaintController is skipping the cache, normally because display items can't
  // be uniquely identified), |id| is nullopt so that the chunk won't match any
  // other chunk.
  using Id = DisplayItem::Id;
  Optional<Id> id;

  // The paint properties which apply to this chunk.
  PaintChunkProperties properties;

  // The total bounds of this paint chunk's contents, in the coordinate space of
  // the containing transform node.
  FloatRect bounds;

  // True if the bounds are filled entirely with opaque contents.
  bool knownToBeOpaque;

  // SPv2 only. Rectangles that need to be re-rasterized in this chunk, in the
  // coordinate space of the containing transform node.
  Vector<FloatRect> rasterInvalidationRects;
};

inline bool operator==(const PaintChunk& a, const PaintChunk& b) {
  return a.beginIndex == b.beginIndex && a.endIndex == b.endIndex &&
         a.id == b.id && a.properties == b.properties && a.bounds == b.bounds &&
         a.knownToBeOpaque == b.knownToBeOpaque &&
         a.rasterInvalidationRects == b.rasterInvalidationRects;
}

inline bool operator!=(const PaintChunk& a, const PaintChunk& b) {
  return !(a == b);
}

inline bool chunkLessThanIndex(const PaintChunk& chunk, size_t index) {
  return chunk.endIndex <= index;
}

inline Vector<PaintChunk>::iterator findChunkInVectorByDisplayItemIndex(
    Vector<PaintChunk>& chunks,
    size_t index) {
  auto chunk =
      std::lower_bound(chunks.begin(), chunks.end(), index, chunkLessThanIndex);
  DCHECK(chunk == chunks.end() ||
         (index >= chunk->beginIndex && index < chunk->endIndex));
  return chunk;
}

inline Vector<PaintChunk>::const_iterator findChunkInVectorByDisplayItemIndex(
    const Vector<PaintChunk>& chunks,
    size_t index) {
  return findChunkInVectorByDisplayItemIndex(
      const_cast<Vector<PaintChunk>&>(chunks), index);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintChunk&, std::ostream*);

}  // namespace blink

#endif  // PaintChunk_h
