/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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
 *
 */

#ifndef LayoutTextControl_h
#define LayoutTextControl_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutFlexibleBox.h"

namespace blink {

class TextControlElement;

class CORE_EXPORT LayoutTextControl : public LayoutBlockFlow {
 public:
  ~LayoutTextControl() override;

  TextControlElement* textControlElement() const;
  virtual PassRefPtr<ComputedStyle> createInnerEditorStyle(
      const ComputedStyle& startStyle) const = 0;

  const char* name() const override { return "LayoutTextControl"; }

 protected:
  LayoutTextControl(TextControlElement*);

  // This convenience function should not be made public because
  // innerEditorElement may outlive the layout tree.
  HTMLElement* innerEditorElement() const;

  int scrollbarThickness() const;
  void adjustInnerEditorStyle(ComputedStyle& textBlockStyle) const;

  void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

  void hitInnerEditorElement(HitTestResult&,
                             const LayoutPoint& pointInContainer,
                             const LayoutPoint& accumulatedOffset);

  int textBlockLogicalWidth() const;
  int textBlockLogicalHeight() const;

  float scaleEmToUnits(int x) const;

  static bool hasValidAvgCharWidth(const SimpleFontData*,
                                   const AtomicString& family);
  virtual float getAvgCharWidth(const AtomicString& family) const;
  virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const = 0;
  virtual LayoutUnit computeControlLogicalHeight(
      LayoutUnit lineHeight,
      LayoutUnit nonContentHeight) const = 0;

  void updateFromElement() override;
  void computeLogicalHeight(LayoutUnit logicalHeight,
                            LayoutUnit logicalTop,
                            LogicalExtentComputedValues&) const override;
  LayoutObject* layoutSpecialExcludedChild(bool relayoutChildren,
                                           SubtreeLayoutScope&) override;

  int firstLineBoxBaseline() const override;
  // We need to override this function because we don't want overflow:hidden on
  // an <input> to affect the baseline calculation. This is necessary because we
  // are an inline-block element as an implementation detail which would
  // normally be affected by this.
  bool shouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }

  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectTextControl || LayoutBlockFlow::isOfType(type);
  }

 private:
  void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth,
                                     LayoutUnit& maxLogicalWidth) const final;
  void computePreferredLogicalWidths() final;
  void removeLeftoverAnonymousBlock(LayoutBlock*) final {}
  bool avoidsFloats() const final { return true; }

  void addOutlineRects(Vector<LayoutRect>&,
                       const LayoutPoint& additionalOffset,
                       IncludeBlockVisualOverflowOrNot) const final;

  bool canBeProgramaticallyScrolled() const final { return true; }
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControl, isTextControl());

// LayoutObject for our inner container, for <search> and others.
// We can't use LayoutFlexibleBox directly, because flexboxes have a different
// baseline definition, and then inputs of different types wouldn't line up
// anymore.
class LayoutTextControlInnerContainer final : public LayoutFlexibleBox {
 public:
  explicit LayoutTextControlInnerContainer(Element* element)
      : LayoutFlexibleBox(element) {}
  ~LayoutTextControlInnerContainer() override {}

  int baselinePosition(FontBaseline baseline,
                       bool firstLine,
                       LineDirectionMode direction,
                       LinePositionMode position) const override {
    return LayoutBlock::baselinePosition(baseline, firstLine, direction,
                                         position);
  }
  int firstLineBoxBaseline() const override {
    return LayoutBlock::firstLineBoxBaseline();
  }
  int inlineBlockBaseline(LineDirectionMode direction) const override {
    return LayoutBlock::inlineBlockBaseline(direction);
  }
  bool shouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }
};

}  // namespace blink

#endif  // LayoutTextControl_h
