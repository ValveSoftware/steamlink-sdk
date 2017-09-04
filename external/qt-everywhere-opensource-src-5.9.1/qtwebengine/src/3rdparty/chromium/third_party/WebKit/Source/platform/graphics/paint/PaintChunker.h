// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunker_h
#define PaintChunker_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

// Accepts information about changes to |PaintChunkProperties| as drawings are
// accumulated, and produces a series of paint chunks: contiguous ranges of the
// display list with identical |PaintChunkProperties|.
class PLATFORM_EXPORT PaintChunker final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(PaintChunker);

 public:
  PaintChunker();
  ~PaintChunker();

  bool isInInitialState() const {
    return m_chunks.isEmpty() && m_currentProperties == PaintChunkProperties();
  }

  const PaintChunkProperties& currentPaintChunkProperties() const {
    return m_currentProperties;
  }
  void updateCurrentPaintChunkProperties(const PaintChunk::Id*,
                                         const PaintChunkProperties&);

  // Returns true if a new chunk is created.
  bool incrementDisplayItemIndex(const DisplayItem&);
  // Returns true if the last chunk is removed.
  bool decrementDisplayItemIndex();

  PaintChunk& paintChunkAt(size_t i) { return m_chunks[i]; }
  size_t lastChunkIndex() const {
    return m_chunks.isEmpty() ? kNotFound : m_chunks.size() - 1;
  }
  PaintChunk& lastChunk() { return m_chunks.last(); }

  PaintChunk& findChunkByDisplayItemIndex(size_t index) {
    auto chunk = findChunkInVectorByDisplayItemIndex(m_chunks, index);
    DCHECK(chunk != m_chunks.end());
    return *chunk;
  }

  void clear();

  // Releases the generated paint chunk list and resets the state of this
  // object.
  Vector<PaintChunk> releasePaintChunks();

 private:
  enum ItemBehavior {
    // Can be combined with adjacent items when building chunks.
    DefaultBehavior = 0,

    // Item requires its own paint chunk.
    RequiresSeparateChunk,
  };

  Vector<PaintChunk> m_chunks;
  Vector<ItemBehavior> m_chunkBehavior;
  Optional<PaintChunk::Id> m_currentChunkId;
  PaintChunkProperties m_currentProperties;
};

#if DCHECK_IS_ON()
class DisableNullPaintPropertyChecks {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DisableNullPaintPropertyChecks);

 public:
  DisableNullPaintPropertyChecks();

 private:
  AutoReset<bool> m_disabler;
};
#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // PaintChunker_h
