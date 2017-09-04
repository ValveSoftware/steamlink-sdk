/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef SVGTextLayoutEngine_h
#define SVGTextLayoutEngine_h

#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGTextFragment.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ComputedStyle;
class InlineFlowBox;
class PathPositionMapper;
class SVGInlineFlowBox;
class SVGInlineTextBox;
class SVGTextMetrics;

// SVGTextLayoutEngine performs the second layout phase for SVG text.
//
// The InlineBox tree was created, containing the text chunk information,
// necessary to apply certain SVG specific text layout properties (text-length
// adjustments and text-anchor).
// The second layout phase uses the SVGTextLayoutAttributes stored in the
// individual LayoutSVGInlineText layoutObjects to compute the final positions
// for each character which are stored in the SVGInlineTextBox objects.

class SVGTextLayoutEngine {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(SVGTextLayoutEngine);

 public:
  SVGTextLayoutEngine(const Vector<LayoutSVGInlineText*>&);
  ~SVGTextLayoutEngine();

  void layoutCharactersInTextBoxes(InlineFlowBox* start);
  void finishLayout();

 private:
  bool setCurrentTextPosition(const SVGCharacterData&);
  void advanceCurrentTextPosition(float glyphAdvance);
  bool applyRelativePositionAdjustmentsIfNeeded(const SVGCharacterData&);

  void computeCurrentFragmentMetrics(SVGInlineTextBox*);
  void recordTextFragment(SVGInlineTextBox*);

  void beginTextPathLayout(SVGInlineFlowBox*);
  void endTextPathLayout();

  void layoutInlineTextBox(SVGInlineTextBox*);
  void layoutTextOnLineOrPath(SVGInlineTextBox*,
                              LineLayoutSVGInlineText,
                              const ComputedStyle&);

  const LayoutSVGInlineText* nextLogicalTextNode();
  const LayoutSVGInlineText* currentLogicalCharacterMetrics(SVGTextMetrics&);
  void advanceToNextLogicalCharacter(const SVGTextMetrics&);

  // Logical iteration state.
  const Vector<LayoutSVGInlineText*>& m_descendantTextNodes;
  unsigned m_currentLogicalTextNodeIndex;
  unsigned m_logicalCharacterOffset;
  unsigned m_logicalMetricsListOffset;

  Vector<SVGInlineTextBox*> m_lineLayoutBoxes;

  SVGTextFragment m_currentTextFragment;
  SVGInlineTextMetricsIterator m_visualMetricsIterator;
  FloatPoint m_textPosition;
  bool m_isVerticalText;
  bool m_inPathLayout;
  bool m_textLengthSpacingInEffect;

  // Text on path layout
  std::unique_ptr<PathPositionMapper> m_textPath;
  float m_textPathStartOffset;
  float m_textPathCurrentOffset;
  float m_textPathDisplacement;
  float m_textPathSpacing;
  float m_textPathScaling;
};

}  // namespace blink

#endif
