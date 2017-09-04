// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemList_h
#define DisplayItemList_h

#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Alignment.h"
#include "wtf/Assertions.h"

namespace blink {

struct PaintChunk;

// kDisplayItemAlignment must be a multiple of alignof(derived display item) for
// each derived display item; the ideal value is the least common multiple.
// Currently the limiting factor is TransformationMatrix (in
// BeginTransform3DDisplayItem), which requests 16-byte alignment.
static const size_t kDisplayItemAlignment =
    WTF_ALIGN_OF(BeginTransform3DDisplayItem);
static const size_t kMaximumDisplayItemSize =
    sizeof(BeginTransform3DDisplayItem);

// A container for a list of display items.
class PLATFORM_EXPORT DisplayItemList
    : public ContiguousContainer<DisplayItem, kDisplayItemAlignment> {
 public:
  DisplayItemList(size_t initialSizeBytes)
      : ContiguousContainer(kMaximumDisplayItemSize, initialSizeBytes) {}
  DisplayItemList(DisplayItemList&& source)
      : ContiguousContainer(std::move(source)),
        m_visualRects(std::move(source.m_visualRects)) {}

  DisplayItemList& operator=(DisplayItemList&& source) {
    ContiguousContainer::operator=(std::move(source));
    m_visualRects = std::move(source.m_visualRects);
    return *this;
  }

  DisplayItem& appendByMoving(DisplayItem&);

  bool hasVisualRect(size_t index) const {
    return index < m_visualRects.size();
  }
  IntRect visualRect(size_t index) const {
    DCHECK(hasVisualRect(index));
    return m_visualRects[index];
  }

  void appendVisualRect(const IntRect& visualRect);

  // Useful for iterating with a range-based for loop.
  template <typename Iterator>
  class Range {
   public:
    Range(const Iterator& begin, const Iterator& end)
        : m_begin(begin), m_end(end) {}
    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }

   private:
    Iterator m_begin;
    Iterator m_end;
  };
  Range<iterator> itemsInPaintChunk(const PaintChunk&);
  Range<const_iterator> itemsInPaintChunk(const PaintChunk&) const;

 private:
  Vector<IntRect> m_visualRects;
};

}  // namespace blink

#endif  // DisplayItemList_h
