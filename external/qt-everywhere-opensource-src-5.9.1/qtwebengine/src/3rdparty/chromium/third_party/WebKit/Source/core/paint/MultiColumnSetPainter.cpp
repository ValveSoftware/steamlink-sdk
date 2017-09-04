// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/MultiColumnSetPainter.h"

#include "core/layout/LayoutMultiColumnSet.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void MultiColumnSetPainter::paintObject(const PaintInfo& paintInfo,
                                        const LayoutPoint& paintOffset) {
  if (m_layoutMultiColumnSet.style()->visibility() != EVisibility::Visible)
    return;

  BlockPainter(m_layoutMultiColumnSet).paintObject(paintInfo, paintOffset);

  // FIXME: Right now we're only painting in the foreground phase.
  // Columns should technically respect phases and allow for
  // background/float/foreground overlap etc., just like LayoutBlocks do. Note
  // this is a pretty minor issue, since the old column implementation clipped
  // columns anyway, thus making it impossible for them to overlap one another.
  // It's also really unlikely that the columns would overlap another block.
  if (!m_layoutMultiColumnSet.flowThread() ||
      (paintInfo.phase != PaintPhaseForeground &&
       paintInfo.phase != PaintPhaseSelection))
    return;

  paintColumnRules(paintInfo, paintOffset);
}

void MultiColumnSetPainter::paintColumnRules(const PaintInfo& paintInfo,
                                             const LayoutPoint& paintOffset) {
  Vector<LayoutRect> columnRuleBounds;
  if (!m_layoutMultiColumnSet.computeColumnRuleBounds(paintOffset,
                                                      columnRuleBounds))
    return;

  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_layoutMultiColumnSet, DisplayItem::kColumnRules))
    return;

  LayoutRect paintRect = m_layoutMultiColumnSet.visualOverflowRect();
  paintRect.moveBy(paintOffset);
  LayoutObjectDrawingRecorder drawingRecorder(
      paintInfo.context, m_layoutMultiColumnSet, DisplayItem::kColumnRules,
      paintRect);

  const ComputedStyle& blockStyle =
      m_layoutMultiColumnSet.multiColumnBlockFlow()->styleRef();
  EBorderStyle ruleStyle = blockStyle.columnRuleStyle();
  bool leftToRight = m_layoutMultiColumnSet.style()->isLeftToRightDirection();
  BoxSide boxSide = m_layoutMultiColumnSet.isHorizontalWritingMode()
                        ? leftToRight ? BSLeft : BSRight
                        : leftToRight ? BSTop : BSBottom;
  const Color& ruleColor = m_layoutMultiColumnSet.resolveColor(
      blockStyle, CSSPropertyColumnRuleColor);

  for (auto& bound : columnRuleBounds) {
    IntRect pixelSnappedRuleRect = pixelSnappedIntRect(bound);
    ObjectPainter::drawLineForBoxSide(
        paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(),
        pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide,
        ruleColor, ruleStyle, 0, 0, true);
  }
}

}  // namespace blink
