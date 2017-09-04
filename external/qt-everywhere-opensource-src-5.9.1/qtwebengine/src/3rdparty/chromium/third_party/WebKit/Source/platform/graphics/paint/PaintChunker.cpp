// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

#if DCHECK_IS_ON()
static bool gNullPaintPropertyChecksDisabled = false;
DisableNullPaintPropertyChecks::DisableNullPaintPropertyChecks()
    : m_disabler(&gNullPaintPropertyChecksDisabled, true) {}
#endif

PaintChunker::PaintChunker() {}

PaintChunker::~PaintChunker() {}

void PaintChunker::updateCurrentPaintChunkProperties(
    const PaintChunk::Id* chunkId,
    const PaintChunkProperties& properties) {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  m_currentChunkId = WTF::nullopt;
  if (chunkId)
    m_currentChunkId.emplace(*chunkId);
  m_currentProperties = properties;
}

bool PaintChunker::incrementDisplayItemIndex(const DisplayItem& item) {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

#if DCHECK_IS_ON()
  if (!gNullPaintPropertyChecksDisabled) {
    // Property nodes should never be null because they should either be set to
    // properties created by a LayoutObject/FrameView, or be set to a non-null
    // root node. If these DCHECKs are hit we are missing a call to update the
    // properties. See: ScopedPaintChunkProperties.
    DCHECK(m_currentProperties.transform);
    DCHECK(m_currentProperties.clip);
    DCHECK(m_currentProperties.effect);
    DCHECK(m_currentProperties.scroll);
  }
#endif

  ItemBehavior behavior;
  Optional<PaintChunk::Id> newChunkId;
  if (DisplayItem::isForeignLayerType(item.getType())) {
    behavior = RequiresSeparateChunk;
    // Use null chunkId if we are skipping cache, so that the chunk will not
    // match any old chunk and will be treated as brand new.
    if (!item.skippedCache())
      newChunkId.emplace(item.getId());

    // Clear m_currentChunkId so that any display items after the foreign layer
    // without a new chunk id will be treated as having no id to avoid the chunk
    // from using the same id as the chunk before the foreign layer chunk.
    m_currentChunkId = WTF::nullopt;
  } else {
    behavior = DefaultBehavior;
    if (!item.skippedCache() && m_currentChunkId)
      newChunkId.emplace(*m_currentChunkId);
  }

  if (m_chunks.isEmpty()) {
    PaintChunk newChunk(0, 1, newChunkId ? &*newChunkId : nullptr,
                        m_currentProperties);
    m_chunks.append(newChunk);
    m_chunkBehavior.append(behavior);
    return true;
  }

  auto& lastChunk = m_chunks.last();
  bool canContinueChunk = m_currentProperties == lastChunk.properties &&
                          behavior != RequiresSeparateChunk &&
                          m_chunkBehavior.last() != RequiresSeparateChunk;
  if (canContinueChunk) {
    lastChunk.endIndex++;
    return false;
  }

  PaintChunk newChunk(lastChunk.endIndex, lastChunk.endIndex + 1,
                      newChunkId ? &*newChunkId : nullptr, m_currentProperties);
  m_chunks.append(newChunk);
  m_chunkBehavior.append(behavior);
  return true;
}

bool PaintChunker::decrementDisplayItemIndex() {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
  DCHECK(!m_chunks.isEmpty());

  auto& lastChunk = m_chunks.last();
  if ((lastChunk.endIndex - lastChunk.beginIndex) > 1) {
    lastChunk.endIndex--;
    return false;
  }

  m_chunks.pop_back();
  m_chunkBehavior.pop_back();
  return true;
}

void PaintChunker::clear() {
  m_chunks.clear();
  m_chunkBehavior.clear();
  m_currentChunkId = WTF::nullopt;
  m_currentProperties = PaintChunkProperties();
}

Vector<PaintChunk> PaintChunker::releasePaintChunks() {
  Vector<PaintChunk> chunks;
  chunks.swap(m_chunks);
  m_chunkBehavior.clear();
  m_currentChunkId = WTF::nullopt;
  m_currentProperties = PaintChunkProperties();
  return chunks;
}

}  // namespace blink
