// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockPainter_h
#define BlockPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class InlineBox;
class LayoutBlock;
class LayoutBox;
class LayoutFlexibleBox;
class LayoutPoint;

class BlockPainter {
  STACK_ALLOCATED();

 public:
  BlockPainter(const LayoutBlock& block) : m_layoutBlock(block) {}

  void paint(const PaintInfo&, const LayoutPoint& paintOffset);
  void paintObject(const PaintInfo&, const LayoutPoint&);
  void paintContents(const PaintInfo&, const LayoutPoint&);
  void paintChildren(const PaintInfo&, const LayoutPoint&);
  void paintChild(const LayoutBox&, const PaintInfo&, const LayoutPoint&);
  void paintOverflowControlsIfNeeded(const PaintInfo&, const LayoutPoint&);

  // See ObjectPainter::paintAllPhasesAtomically().
  void paintAllChildPhasesAtomically(const LayoutBox&,
                                     const PaintInfo&,
                                     const LayoutPoint&);
  static void paintChildrenOfFlexibleBox(const LayoutFlexibleBox&,
                                         const PaintInfo&,
                                         const LayoutPoint& paintOffset);
  static void paintInlineBox(const InlineBox&,
                             const PaintInfo&,
                             const LayoutPoint& paintOffset);

  // The adjustedPaintOffset should include the location (offset) of the object
  // itself.
  bool intersectsPaintRect(const PaintInfo&,
                           const LayoutPoint& adjustedPaintOffset) const;

 private:
  void paintCarets(const PaintInfo&, const LayoutPoint&);

  const LayoutBlock& m_layoutBlock;
};

}  // namespace blink

#endif  // BlockPainter_h
