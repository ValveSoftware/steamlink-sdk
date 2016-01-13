/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef RenderTextControl_h
#define RenderTextControl_h

#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/RenderFlexibleBox.h"

namespace WebCore {

class HTMLTextFormControlElement;

class RenderTextControl : public RenderBlockFlow {
public:
    virtual ~RenderTextControl();

    HTMLTextFormControlElement* textFormControlElement() const;
    virtual PassRefPtr<RenderStyle> createInnerEditorStyle(const RenderStyle* startStyle) const = 0;

protected:
    RenderTextControl(HTMLTextFormControlElement*);

    // This convenience function should not be made public because
    // innerEditorElement may outlive the render tree.
    HTMLElement* innerEditorElement() const;

    int scrollbarThickness() const;
    void adjustInnerEditorStyle(RenderStyle* textBlockStyle) const;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    void hitInnerEditorElement(HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset);

    int textBlockLogicalWidth() const;
    int textBlockLogicalHeight() const;

    float scaleEmToUnits(int x) const;

    static bool hasValidAvgCharWidth(AtomicString family);
    virtual float getAvgCharWidth(AtomicString family);
    virtual LayoutUnit preferredContentLogicalWidth(float charWidth) const = 0;
    virtual LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const = 0;

    virtual void updateFromElement() OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;
    virtual RenderObject* layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope&) OVERRIDE;

    // We need to override this function because we don't want overflow:hidden on an <input>
    // to affect the baseline calculation. This is necessary because we are an inline-block
    // element as an implementation detail which would normally be affected by this.
    virtual int inlineBlockBaseline(LineDirectionMode direction) const OVERRIDE { return lastLineBoxBaseline(direction); }

private:
    virtual const char* renderName() const OVERRIDE { return "RenderTextControl"; }
    virtual bool isTextControl() const OVERRIDE FINAL { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE FINAL;
    virtual void computePreferredLogicalWidths() OVERRIDE FINAL;
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) OVERRIDE FINAL { }
    virtual bool avoidsFloats() const OVERRIDE FINAL { return true; }
    virtual bool canHaveGeneratedChildren() const OVERRIDE FINAL { return false; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) OVERRIDE FINAL;

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) OVERRIDE FINAL;

    virtual bool canBeProgramaticallyScrolled() const OVERRIDE FINAL { return true; }
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderTextControl, isTextControl());

// Renderer for our inner container, for <search> and others.
// We can't use RenderFlexibleBox directly, because flexboxes have a different
// baseline definition, and then inputs of different types wouldn't line up
// anymore.
class RenderTextControlInnerContainer FINAL : public RenderFlexibleBox {
public:
    explicit RenderTextControlInnerContainer(Element* element)
        : RenderFlexibleBox(element)
    { }
    virtual ~RenderTextControlInnerContainer() { }

    virtual int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const OVERRIDE
    {
        return RenderBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    virtual int firstLineBoxBaseline() const OVERRIDE { return RenderBlock::firstLineBoxBaseline(); }
    virtual int inlineBlockBaseline(LineDirectionMode direction) const OVERRIDE { return lastLineBoxBaseline(direction); }
};


} // namespace WebCore

#endif // RenderTextControl_h
