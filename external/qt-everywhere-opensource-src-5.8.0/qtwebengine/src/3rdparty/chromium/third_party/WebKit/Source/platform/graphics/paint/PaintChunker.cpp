// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

PaintChunker::PaintChunker()
{
}

PaintChunker::~PaintChunker()
{
}

void PaintChunker::updateCurrentPaintChunkProperties(const PaintChunkProperties& properties)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    m_currentProperties = properties;
}

void PaintChunker::incrementDisplayItemIndex(ItemBehavior behavior)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    if (m_chunks.isEmpty()) {
        PaintChunk newChunk(0, 1, m_currentProperties);
        m_chunks.append(newChunk);
        m_chunkBehavior.append(behavior);
        return;
    }

    auto& lastChunk = m_chunks.last();
    bool canContinueChunk = m_currentProperties == lastChunk.properties
        && behavior != RequiresSeparateChunk
        && m_chunkBehavior.last() != RequiresSeparateChunk;
    if (canContinueChunk) {
        lastChunk.endIndex++;
        return;
    }

    PaintChunk newChunk(lastChunk.endIndex, lastChunk.endIndex + 1, m_currentProperties);
    m_chunks.append(newChunk);
    m_chunkBehavior.append(behavior);
}

void PaintChunker::decrementDisplayItemIndex()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    ASSERT(!m_chunks.isEmpty());

    auto& lastChunk = m_chunks.last();
    if ((lastChunk.endIndex - lastChunk.beginIndex) > 1) {
        lastChunk.endIndex--;
    } else {
        m_chunks.removeLast();
        m_chunkBehavior.removeLast();
    }
}

void PaintChunker::clear()
{
    m_chunks.clear();
    m_chunkBehavior.clear();
    m_currentProperties = PaintChunkProperties();
}

Vector<PaintChunk> PaintChunker::releasePaintChunks()
{
    Vector<PaintChunk> chunks;
    chunks.swap(m_chunks);
    m_chunkBehavior.clear();
    m_currentProperties = PaintChunkProperties();
    return chunks;
}

} // namespace blink
