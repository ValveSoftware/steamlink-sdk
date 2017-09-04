/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef LayoutTextControlSingleLine_h
#define LayoutTextControlSingleLine_h

#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutTextControl.h"

namespace blink {

class HTMLInputElement;

class LayoutTextControlSingleLine : public LayoutTextControl {
 public:
  LayoutTextControlSingleLine(HTMLInputElement*);
  ~LayoutTextControlSingleLine() override;
  // FIXME: Move createInnerEditorStyle() to TextControlInnerEditorElement.
  PassRefPtr<ComputedStyle> createInnerEditorStyle(
      const ComputedStyle& startStyle) const final;

  void capsLockStateMayHaveChanged();

 protected:
  Element* containerElement() const;
  Element* editingViewPortElement() const;
  HTMLInputElement* inputElement() const;

 private:
  bool hasControlClip() const final;
  LayoutRect controlClipRect(const LayoutPoint&) const final;
  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectTextField || LayoutTextControl::isOfType(type);
  }

  void paint(const PaintInfo&, const LayoutPoint&) const override;
  void layout() override;

  bool nodeAtPoint(HitTestResult&,
                   const HitTestLocation& locationInContainer,
                   const LayoutPoint& accumulatedOffset,
                   HitTestAction) final;

  void autoscroll(const IntPoint&) final;

  // Subclassed to forward to our inner div.
  LayoutUnit scrollLeft() const final;
  LayoutUnit scrollTop() const final;
  LayoutUnit scrollWidth() const final;
  LayoutUnit scrollHeight() const final;
  void setScrollLeft(LayoutUnit) final;
  void setScrollTop(LayoutUnit) final;

  int textBlockWidth() const;
  float getAvgCharWidth(const AtomicString& family) const final;
  LayoutUnit preferredContentLogicalWidth(float charWidth) const final;
  LayoutUnit computeControlLogicalHeight(
      LayoutUnit lineHeight,
      LayoutUnit nonContentHeight) const override;
  void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) final;
  void addOverflowFromChildren() final;

  bool allowsOverflowClip() const override { return false; }

  bool textShouldBeTruncated() const;
  HTMLElement* innerSpinButtonElement() const;

  bool m_shouldDrawCapsLockIndicator;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControlSingleLine, isTextField());

// ----------------------------

class LayoutTextControlInnerEditor : public LayoutBlockFlow {
 public:
  LayoutTextControlInnerEditor(Element* element) : LayoutBlockFlow(element) {}
  bool shouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }

 private:
  bool isIntrinsicallyScrollable(
      ScrollbarOrientation orientation) const override {
    return orientation == HorizontalScrollbar;
  }
  bool scrollsOverflowX() const override { return hasOverflowClip(); }
  bool scrollsOverflowY() const override { return false; }
  bool hasLineIfEmpty() const override { return true; }
};

}  // namespace blink

#endif
