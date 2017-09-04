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

DisplayItem& DisplayItemList::appendByMoving(DisplayItem& item) {
#ifndef NDEBUG
  String originalDebugString = item.asDebugString();
#endif
  ASSERT(item.hasValidClient());
  DisplayItem& result =
      ContiguousContainer::appendByMoving(item, item.derivedSize());
  // ContiguousContainer::appendByMoving() calls an in-place constructor
  // on item which replaces it with a tombstone/"dead display item" that
  // can be safely destructed but should never be used.
  ASSERT(!item.hasValidClient());
#ifndef NDEBUG
  // Save original debug string in the old item to help debugging.
  item.setClientDebugString(originalDebugString);
#endif
  return result;
}

void DisplayItemList::appendVisualRect(const IntRect& visualRect) {
  m_visualRects.append(visualRect);
}

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) {
  return Range<iterator>(begin() + paintChunk.beginIndex,
                         begin() + paintChunk.endIndex);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) const {
  return Range<const_iterator>(begin() + paintChunk.beginIndex,
                               begin() + paintChunk.endIndex);
}

}  // namespace blink
