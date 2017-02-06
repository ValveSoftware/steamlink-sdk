/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutTextControlMultiLine_h
#define LayoutTextControlMultiLine_h

#include "core/layout/LayoutTextControl.h"

namespace blink {

class HTMLTextAreaElement;

class LayoutTextControlMultiLine final : public LayoutTextControl {
public:
    LayoutTextControlMultiLine(HTMLTextAreaElement*);
    ~LayoutTextControlMultiLine() override;

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectTextArea || LayoutTextControl::isOfType(type); }

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    float getAvgCharWidth(const AtomicString& family) const override;
    LayoutUnit preferredContentLogicalWidth(float charWidth) const override;
    LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
    // We override the two baseline functions because we want our baseline to be the bottom of our margin box.
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    int inlineBlockBaseline(LineDirectionMode) const override { return -1; }

    PassRefPtr<ComputedStyle> createInnerEditorStyle(const ComputedStyle& startStyle) const override;
    LayoutObject* layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope&) override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControlMultiLine, isTextArea());

} // namespace blink

#endif
