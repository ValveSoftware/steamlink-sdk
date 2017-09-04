// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/EllipsisBoxPainter.h"

#include "core/layout/TextRunConstructor.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TextPainter.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void EllipsisBoxPainter::paint(const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset,
                               LayoutUnit lineTop,
                               LayoutUnit lineBottom) {
  if (paintInfo.phase == PaintPhaseSelection)
    return;

  const ComputedStyle& style = m_ellipsisBox.getLineLayoutItem().styleRef(
      m_ellipsisBox.isFirstLineStyle());
  paintEllipsis(paintInfo, paintOffset, lineTop, lineBottom, style);
}

void EllipsisBoxPainter::paintEllipsis(const PaintInfo& paintInfo,
                                       const LayoutPoint& paintOffset,
                                       LayoutUnit lineTop,
                                       LayoutUnit lineBottom,
                                       const ComputedStyle& style) {
  LayoutRect paintRect(m_ellipsisBox.logicalFrameRect());
  m_ellipsisBox.logicalRectToPhysicalRect(paintRect);
  paintRect.moveBy(paintOffset);

  GraphicsContext& context = paintInfo.context;
  DisplayItem::Type displayItemType =
      DisplayItem::paintPhaseToDrawingType(paintInfo.phase);
  if (DrawingRecorder::useCachedDrawingIfPossible(context, m_ellipsisBox,
                                                  displayItemType))
    return;

  DrawingRecorder recorder(context, m_ellipsisBox, displayItemType,
                           FloatRect(paintRect));

  LayoutPoint boxOrigin = m_ellipsisBox.locationIncludingFlipping();
  boxOrigin.moveBy(paintOffset);
  LayoutRect boxRect(boxOrigin,
                     LayoutSize(m_ellipsisBox.logicalWidth(),
                                m_ellipsisBox.virtualLogicalHeight()));

  GraphicsContextStateSaver stateSaver(context);
  if (!m_ellipsisBox.isHorizontal())
    context.concatCTM(TextPainter::rotation(boxRect, TextPainter::Clockwise));

  const Font& font = style.font();
  const SimpleFontData* fontData = font.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return;

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      m_ellipsisBox.getLineLayoutItem(), style, paintInfo);
  TextRun textRun = constructTextRun(font, m_ellipsisBox.ellipsisStr(), style,
                                     TextRun::AllowTrailingExpansion);
  LayoutPoint textOrigin(boxOrigin.x(),
                         boxOrigin.y() + fontData->getFontMetrics().ascent());
  TextPainter textPainter(context, font, textRun, textOrigin, boxRect,
                          m_ellipsisBox.isHorizontal());
  textPainter.paint(0, m_ellipsisBox.ellipsisStr().length(),
                    m_ellipsisBox.ellipsisStr().length(), textStyle);
}

}  // namespace blink
