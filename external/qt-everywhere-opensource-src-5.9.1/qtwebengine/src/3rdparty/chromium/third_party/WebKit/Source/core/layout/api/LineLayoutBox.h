// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBox_h
#define LineLayoutBox_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBox;

class LineLayoutBox : public LineLayoutBoxModel {
 public:
  explicit LineLayoutBox(LayoutBox* layoutBox)
      : LineLayoutBoxModel(layoutBox) {}

  explicit LineLayoutBox(const LineLayoutItem& item)
      : LineLayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.isBox());
  }

  explicit LineLayoutBox(std::nullptr_t) : LineLayoutBoxModel(nullptr) {}

  LineLayoutBox() {}

  LayoutPoint location() const { return toBox()->location(); }

  LayoutSize size() const { return toBox()->size(); }

  void setLogicalHeight(LayoutUnit size) { toBox()->setLogicalHeight(size); }

  LayoutUnit logicalHeight() const { return toBox()->logicalHeight(); }

  LayoutUnit logicalTop() const { return toBox()->logicalTop(); }

  LayoutUnit logicalBottom() const { return toBox()->logicalBottom(); }

  LayoutUnit flipForWritingMode(LayoutUnit unit) const {
    return toBox()->flipForWritingMode(unit);
  }

  void flipForWritingMode(FloatRect& rect) const {
    toBox()->flipForWritingMode(rect);
  }

  FloatPoint flipForWritingMode(const FloatPoint& point) const {
    return toBox()->flipForWritingMode(point);
  }

  void flipForWritingMode(LayoutRect& rect) const {
    toBox()->flipForWritingMode(rect);
  }

  LayoutPoint flipForWritingMode(const LayoutPoint& point) const {
    return toBox()->flipForWritingMode(point);
  }

  LayoutPoint flipForWritingModeForChild(const LineLayoutBox& child,
                                         LayoutPoint childPoint) const {
    return toBox()->flipForWritingModeForChild(
        toLayoutBox(child.layoutObject()), childPoint);
  }

  void moveWithEdgeOfInlineContainerIfNecessary(bool isHorizontal) {
    toBox()->moveWithEdgeOfInlineContainerIfNecessary(isHorizontal);
  }

  void move(const LayoutUnit& width, const LayoutUnit& height) {
    toBox()->move(width, height);
  }

  bool hasOverflowModel() const { return toBox()->hasOverflowModel(); }
  LayoutRect logicalVisualOverflowRectForPropagation(
      const ComputedStyle& style) const {
    return toBox()->logicalVisualOverflowRectForPropagation(style);
  }
  LayoutRect logicalLayoutOverflowRectForPropagation(
      const ComputedStyle& style) const {
    return toBox()->logicalLayoutOverflowRectForPropagation(style);
  }

  void setLocation(const LayoutPoint& location) {
    return toBox()->setLocation(location);
  }

  void setSize(const LayoutSize& size) { return toBox()->setSize(size); }

  IntSize scrolledContentOffset() const {
    return toBox()->scrolledContentOffset();
  }

  InlineBox* createInlineBox() { return toBox()->createInlineBox(); }

  InlineBox* inlineBoxWrapper() const { return toBox()->inlineBoxWrapper(); }

  void setInlineBoxWrapper(InlineBox* box) {
    return toBox()->setInlineBoxWrapper(box);
  }

#ifndef NDEBUG

  void showLineTreeAndMark(const InlineBox* markedBox1,
                           const char* markedLabel1) const {
    if (layoutObject()->isLayoutBlockFlow())
      toLayoutBlockFlow(layoutObject())
          ->showLineTreeAndMark(markedBox1, markedLabel1);
  }

#endif

 private:
  LayoutBox* toBox() { return toLayoutBox(layoutObject()); }

  const LayoutBox* toBox() const { return toLayoutBox(layoutObject()); }
};

inline LineLayoutBox LineLayoutItem::containingBlock() const {
  return LineLayoutBox(layoutObject()->containingBlock());
}

}  // namespace blink

#endif  // LineLayoutBox_h
