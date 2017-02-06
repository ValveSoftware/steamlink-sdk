// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

DisplayItem& DisplayItemList::appendByMoving(DisplayItem& item, const IntRect& visualRect, SkPictureGpuAnalyzer& gpuAnalyzer)
{
    // No reason to continue the analysis once we have a veto.
    if (gpuAnalyzer.suitableForGpuRasterization())
        item.analyzeForGpuRasterization(gpuAnalyzer);

#ifndef NDEBUG
    String originalDebugString = item.asDebugString();
#endif
    ASSERT(item.hasValidClient());
    DisplayItem& result = ContiguousContainer::appendByMoving(item, item.derivedSize());
    // ContiguousContainer::appendByMoving() calls an in-place constructor
    // on item which replaces it with a tombstone/"dead display item" that
    // can be safely destructed but should never be used.
    ASSERT(!item.hasValidClient());
#ifndef NDEBUG
    // Save original debug string in the old item to help debugging.
    item.setClientDebugString(originalDebugString);
#endif
    appendVisualRect(visualRect);
    return result;
}

void DisplayItemList::appendVisualRect(const IntRect& visualRect)
{
    size_t itemIndex = m_visualRects.size();
    const DisplayItem& item = (*this)[itemIndex];

    // For paired display items such as transforms, since we are not guaranteed containment, the
    // visual rect must comprise the union of the visual rects for all items within its block.

    if (item.isBegin()) {
        m_visualRects.append(visualRect);
        m_beginItemIndices.append(itemIndex);

    } else if (item.isEnd()) {
        size_t lastBeginIndex = m_beginItemIndices.last();
        m_beginItemIndices.removeLast();

        // Ending bounds match the starting bounds.
        m_visualRects.append(m_visualRects[lastBeginIndex]);

        // The block that ended needs to be included in the bounds of the enclosing block.
        growCurrentBeginItemVisualRect(m_visualRects[lastBeginIndex]);

    } else {
        m_visualRects.append(visualRect);
        growCurrentBeginItemVisualRect(visualRect);
    }
}

void DisplayItemList::growCurrentBeginItemVisualRect(const IntRect& visualRect)
{
    if (!m_beginItemIndices.isEmpty())
        m_visualRects[m_beginItemIndices.last()].unite(visualRect);
}

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk)
{
    return Range<iterator>(begin() + paintChunk.beginIndex, begin() + paintChunk.endIndex);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) const
{
    return Range<const_iterator>(begin() + paintChunk.beginIndex, begin() + paintChunk.endIndex);
}

} // namespace blink
