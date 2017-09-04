/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/line/LineWidth.h"

#include "core/layout/api/LineLayoutRubyRun.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"

namespace blink {

LineWidth::LineWidth(LineLayoutBlockFlow block,
                     bool isFirstLine,
                     IndentTextOrNot indentText)
    : m_block(block),
      m_uncommittedWidth(0),
      m_committedWidth(0),
      m_overhangWidth(0),
      m_trailingWhitespaceWidth(0),
      m_isFirstLine(isFirstLine),
      m_indentText(indentText) {
  updateAvailableWidth();
}

void LineWidth::updateAvailableWidth(LayoutUnit replacedHeight) {
  LayoutUnit height = m_block.logicalHeight();
  LayoutUnit logicalHeight =
      m_block.minLineHeightForReplacedObject(m_isFirstLine, replacedHeight);
  m_left =
      m_block.logicalLeftOffsetForLine(height, indentText(), logicalHeight);
  m_right =
      m_block.logicalRightOffsetForLine(height, indentText(), logicalHeight);

  computeAvailableWidthFromLeftAndRight();
}

void LineWidth::shrinkAvailableWidthForNewFloatIfNeeded(
    const FloatingObject& newFloat) {
  LayoutUnit height = m_block.logicalHeight();
  if (height < m_block.logicalTopForFloat(newFloat) ||
      height >= m_block.logicalBottomForFloat(newFloat))
    return;

  ShapeOutsideDeltas shapeDeltas;
  if (ShapeOutsideInfo* shapeOutsideInfo =
          newFloat.layoutObject()->shapeOutsideInfo()) {
    LayoutUnit lineHeight = m_block.lineHeight(
        m_isFirstLine,
        m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine,
        PositionOfInteriorLineBoxes);
    shapeDeltas = shapeOutsideInfo->computeDeltasForContainingBlockLine(
        m_block, newFloat, m_block.logicalHeight(), lineHeight);
  }

  if (newFloat.getType() == FloatingObject::FloatLeft) {
    LayoutUnit newLeft = m_block.logicalRightForFloat(newFloat);
    if (shapeDeltas.isValid()) {
      if (shapeDeltas.lineOverlapsShape()) {
        newLeft += shapeDeltas.rightMarginBoxDelta();
      } else {
        // Per the CSS Shapes spec, If the line doesn't overlap the shape, then
        // ignore this shape for this line.
        newLeft = m_left;
      }
    }
    if (indentText() == IndentText && m_block.style()->isLeftToRightDirection())
      newLeft += floorToInt(m_block.textIndentOffset());
    m_left = std::max(m_left, newLeft);
  } else {
    LayoutUnit newRight = m_block.logicalLeftForFloat(newFloat);
    if (shapeDeltas.isValid()) {
      if (shapeDeltas.lineOverlapsShape()) {
        newRight += shapeDeltas.leftMarginBoxDelta();
      } else {
        // Per the CSS Shapes spec, If the line doesn't overlap the shape, then
        // ignore this shape for this line.
        newRight = m_right;
      }
    }
    if (indentText() == IndentText &&
        !m_block.style()->isLeftToRightDirection())
      newRight -= floorToInt(m_block.textIndentOffset());
    m_right = std::min(m_right, newRight);
  }

  computeAvailableWidthFromLeftAndRight();
}

void LineWidth::commit() {
  m_committedWidth += m_uncommittedWidth;
  m_uncommittedWidth = 0;
}

void LineWidth::applyOverhang(LineLayoutRubyRun rubyRun,
                              LineLayoutItem startLayoutItem,
                              LineLayoutItem endLayoutItem) {
  int startOverhang;
  int endOverhang;
  rubyRun.getOverhang(m_isFirstLine, startLayoutItem, endLayoutItem,
                      startOverhang, endOverhang);

  startOverhang = std::min<int>(startOverhang, m_committedWidth);
  m_availableWidth += startOverhang;

  endOverhang = std::max(
      std::min<int>(endOverhang, m_availableWidth - currentWidth()), 0);
  m_availableWidth += endOverhang;
  m_overhangWidth += startOverhang + endOverhang;
}

inline static LayoutUnit availableWidthAtOffset(
    LineLayoutBlockFlow block,
    const LayoutUnit& offset,
    IndentTextOrNot indentText,
    LayoutUnit& newLineLeft,
    LayoutUnit& newLineRight,
    const LayoutUnit& lineHeight = LayoutUnit()) {
  newLineLeft = block.logicalLeftOffsetForLine(offset, indentText, lineHeight);
  newLineRight =
      block.logicalRightOffsetForLine(offset, indentText, lineHeight);
  return (newLineRight - newLineLeft).clampNegativeToZero();
}

void LineWidth::updateLineDimension(LayoutUnit newLineTop,
                                    LayoutUnit newLineWidth,
                                    const LayoutUnit& newLineLeft,
                                    const LayoutUnit& newLineRight) {
  if (newLineWidth <= m_availableWidth)
    return;

  m_block.setLogicalHeight(newLineTop);
  m_availableWidth = newLineWidth + LayoutUnit::fromFloatCeil(m_overhangWidth);
  m_left = newLineLeft;
  m_right = newLineRight;
}

void LineWidth::wrapNextToShapeOutside(bool isFirstLine) {
  LayoutUnit lineHeight = m_block.lineHeight(
      isFirstLine,
      m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine,
      PositionOfInteriorLineBoxes);
  LayoutUnit lineLogicalTop = m_block.logicalHeight();
  LayoutUnit newLineTop = lineLogicalTop;
  LayoutUnit floatLogicalBottom =
      m_block.nextFloatLogicalBottomBelow(lineLogicalTop);

  LayoutUnit newLineWidth;
  LayoutUnit newLineLeft = m_left;
  LayoutUnit newLineRight = m_right;
  while (true) {
    newLineWidth =
        availableWidthAtOffset(m_block, newLineTop, indentText(), newLineLeft,
                               newLineRight, lineHeight);
    if (newLineWidth >= m_uncommittedWidth)
      break;

    if (newLineTop >= floatLogicalBottom)
      break;

    newLineTop++;
  }
  updateLineDimension(newLineTop, LayoutUnit(newLineWidth), newLineLeft,
                      newLineRight);
}

void LineWidth::fitBelowFloats(bool isFirstLine) {
  ASSERT(!m_committedWidth);
  ASSERT(!fitsOnLine());
  m_block.placeNewFloats(m_block.logicalHeight(), this);

  LayoutUnit floatLogicalBottom;
  LayoutUnit lastFloatLogicalBottom = m_block.logicalHeight();
  LayoutUnit newLineWidth = m_availableWidth;
  LayoutUnit newLineLeft = m_left;
  LayoutUnit newLineRight = m_right;

  FloatingObject* lastFloatFromPreviousLine =
      m_block.lastFloatFromPreviousLine();
  if (lastFloatFromPreviousLine &&
      lastFloatFromPreviousLine->layoutObject()->shapeOutsideInfo())
    return wrapNextToShapeOutside(isFirstLine);

  while (true) {
    floatLogicalBottom =
        m_block.nextFloatLogicalBottomBelow(lastFloatLogicalBottom);
    if (floatLogicalBottom <= lastFloatLogicalBottom)
      break;

    newLineWidth = availableWidthAtOffset(
        m_block, floatLogicalBottom, indentText(), newLineLeft, newLineRight);
    lastFloatLogicalBottom = floatLogicalBottom;

    if (newLineWidth >= m_uncommittedWidth)
      break;
  }
  updateLineDimension(lastFloatLogicalBottom, LayoutUnit(newLineWidth),
                      newLineLeft, newLineRight);
}

void LineWidth::computeAvailableWidthFromLeftAndRight() {
  m_availableWidth = (m_right - m_left).clampNegativeToZero() +
                     LayoutUnit::fromFloatCeil(m_overhangWidth);
}

}  // namespace blink
