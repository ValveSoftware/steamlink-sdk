/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/SVGTextLayoutEngineBaseline.h"

#include "core/style/SVGComputedStyle.h"
#include "core/svg/SVGLengthContext.h"
#include "platform/fonts/Font.h"

namespace blink {

SVGTextLayoutEngineBaseline::SVGTextLayoutEngineBaseline(const Font& font,
                                                         float effectiveZoom)
    : m_font(font), m_effectiveZoom(effectiveZoom) {
  ASSERT(m_effectiveZoom);
}

float SVGTextLayoutEngineBaseline::calculateBaselineShift(
    const ComputedStyle& style) const {
  const SVGComputedStyle& svgStyle = style.svgStyle();
  const SimpleFontData* fontData = m_font.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return 0;

  DCHECK(m_effectiveZoom);
  switch (svgStyle.baselineShift()) {
    case BS_LENGTH:
      return SVGLengthContext::valueForLength(
          svgStyle.baselineShiftValue(), style,
          m_font.getFontDescription().computedPixelSize() / m_effectiveZoom);
    case BS_SUB:
      return -fontData->getFontMetrics().floatHeight() / 2 / m_effectiveZoom;
    case BS_SUPER:
      return fontData->getFontMetrics().floatHeight() / 2 / m_effectiveZoom;
    default:
      ASSERT_NOT_REACHED();
      return 0;
  }
}

EAlignmentBaseline
SVGTextLayoutEngineBaseline::dominantBaselineToAlignmentBaseline(
    bool isVerticalText,
    LineLayoutItem textLineLayout) const {
  ASSERT(textLineLayout);
  ASSERT(textLineLayout.style());

  const SVGComputedStyle& style = textLineLayout.style()->svgStyle();

  EDominantBaseline baseline = style.dominantBaseline();
  if (baseline == DB_AUTO) {
    if (isVerticalText)
      baseline = DB_CENTRAL;
    else
      baseline = DB_ALPHABETIC;
  }

  switch (baseline) {
    case DB_USE_SCRIPT:
      // TODO(fs): The dominant-baseline and the baseline-table components
      // are set by determining the predominant script of the character data
      // content.
      return AB_ALPHABETIC;
    case DB_NO_CHANGE:
      ASSERT(textLineLayout.parent());
      return dominantBaselineToAlignmentBaseline(isVerticalText,
                                                 textLineLayout.parent());
    case DB_RESET_SIZE:
      ASSERT(textLineLayout.parent());
      return dominantBaselineToAlignmentBaseline(isVerticalText,
                                                 textLineLayout.parent());
    case DB_IDEOGRAPHIC:
      return AB_IDEOGRAPHIC;
    case DB_ALPHABETIC:
      return AB_ALPHABETIC;
    case DB_HANGING:
      return AB_HANGING;
    case DB_MATHEMATICAL:
      return AB_MATHEMATICAL;
    case DB_CENTRAL:
      return AB_CENTRAL;
    case DB_MIDDLE:
      return AB_MIDDLE;
    case DB_TEXT_AFTER_EDGE:
      return AB_TEXT_AFTER_EDGE;
    case DB_TEXT_BEFORE_EDGE:
      return AB_TEXT_BEFORE_EDGE;
    default:
      ASSERT_NOT_REACHED();
      return AB_AUTO;
  }
}

float SVGTextLayoutEngineBaseline::calculateAlignmentBaselineShift(
    bool isVerticalText,
    LineLayoutItem textLineLayout) const {
  ASSERT(textLineLayout);
  ASSERT(textLineLayout.style());
  ASSERT(textLineLayout.parent());

  LineLayoutItem textLineLayoutParent = textLineLayout.parent();
  ASSERT(textLineLayoutParent);

  EAlignmentBaseline baseline =
      textLineLayout.style()->svgStyle().alignmentBaseline();
  if (baseline == AB_AUTO || baseline == AB_BASELINE) {
    baseline = dominantBaselineToAlignmentBaseline(isVerticalText,
                                                   textLineLayoutParent);
    ASSERT(baseline != AB_AUTO && baseline != AB_BASELINE);
  }

  const SimpleFontData* fontData = m_font.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return 0;

  const FontMetrics& fontMetrics = fontData->getFontMetrics();
  float ascent = fontMetrics.floatAscent() / m_effectiveZoom;
  float descent = fontMetrics.floatDescent() / m_effectiveZoom;
  float xheight = fontMetrics.xHeight() / m_effectiveZoom;

  // Note: http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
  switch (baseline) {
    case AB_BEFORE_EDGE:
    case AB_TEXT_BEFORE_EDGE:
      return ascent;
    case AB_MIDDLE:
      return xheight / 2;
    case AB_CENTRAL:
      return (ascent - descent) / 2;
    case AB_AFTER_EDGE:
    case AB_TEXT_AFTER_EDGE:
    case AB_IDEOGRAPHIC:
      return -descent;
    case AB_ALPHABETIC:
      return 0;
    case AB_HANGING:
      return ascent * 8 / 10.f;
    case AB_MATHEMATICAL:
      return ascent / 2;
    case AB_BASELINE:
    default:
      ASSERT_NOT_REACHED();
      return 0;
  }
}

}  // namespace blink
