// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBoxModel_h
#define LineLayoutBoxModel_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutItem.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBoxModelObject;

class LineLayoutBoxModel : public LineLayoutItem {
public:
    explicit LineLayoutBoxModel(LayoutBoxModelObject* layoutBox)
        : LineLayoutItem(layoutBox)
    {
    }

    explicit LineLayoutBoxModel(const LineLayoutItem& item)
        : LineLayoutItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isBoxModelObject());
    }

    explicit LineLayoutBoxModel(std::nullptr_t) : LineLayoutItem(nullptr) { }

    LineLayoutBoxModel() { }

    // TODO(dgrogan) Remove. Implement API methods that proxy to the PaintLayer.
    PaintLayer* layer() const
    {
        return toBoxModel()->layer();
    }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode = PositionOnContainingLine) const
    {
        return toBoxModel()->lineHeight(firstLine, lineDirectionMode, linePositionMode);
    }

    int baselinePosition(FontBaseline fontBaseline, bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode = PositionOnContainingLine) const
    {
        return toBoxModel()->baselinePosition(fontBaseline, firstLine, lineDirectionMode, linePositionMode);
    }

    bool hasSelfPaintingLayer() const
    {
        return toBoxModel()->hasSelfPaintingLayer();
    }

    LayoutUnit marginTop() const
    {
        return toBoxModel()->marginTop();
    }

    LayoutUnit marginBottom() const
    {
        return toBoxModel()->marginBottom();
    }

    LayoutUnit marginLeft() const
    {
        return toBoxModel()->marginLeft();
    }

    LayoutUnit marginRight() const
    {
        return toBoxModel()->marginRight();
    }

    LayoutUnit marginBefore(const ComputedStyle* otherStyle = nullptr) const
    {
        return toBoxModel()->marginBefore(otherStyle);
    }

    LayoutUnit marginAfter(const ComputedStyle* otherStyle = nullptr) const
    {
        return toBoxModel()->marginAfter(otherStyle);
    }

    LayoutUnit marginOver() const
    {
        return toBoxModel()->marginOver();
    }

    LayoutUnit marginUnder() const
    {
        return toBoxModel()->marginUnder();
    }

    LayoutUnit paddingTop() const
    {
        return toBoxModel()->paddingTop();
    }

    LayoutUnit paddingBottom() const
    {
        return toBoxModel()->paddingBottom();
    }

    LayoutUnit paddingLeft() const
    {
        return toBoxModel()->paddingLeft();
    }

    LayoutUnit paddingRight() const
    {
        return toBoxModel()->paddingRight();
    }

    LayoutUnit paddingBefore() const
    {
        return toBoxModel()->paddingBefore();
    }

    LayoutUnit paddingAfter() const
    {
        return toBoxModel()->paddingAfter();
    }

    int borderTop() const
    {
        return toBoxModel()->borderTop();
    }

    int borderBottom() const
    {
        return toBoxModel()->borderBottom();
    }

    int borderLeft() const
    {
        return toBoxModel()->borderLeft();
    }

    int borderRight() const
    {
        return toBoxModel()->borderRight();
    }

    int borderBefore() const
    {
        return toBoxModel()->borderBefore();
    }

    int borderAfter() const
    {
        return toBoxModel()->borderAfter();
    }

    LayoutSize relativePositionLogicalOffset() const
    {
        return toBoxModel()->relativePositionLogicalOffset();
    }

    bool hasInlineDirectionBordersOrPadding() const
    {
        return toBoxModel()->hasInlineDirectionBordersOrPadding();
    }

    LayoutUnit borderAndPaddingOver() const
    {
        return toBoxModel()->borderAndPaddingOver();
    }

    LayoutUnit borderAndPaddingUnder() const
    {
        return toBoxModel()->borderAndPaddingUnder();
    }

    LayoutUnit borderAndPaddingLogicalHeight() const
    {
        return toBoxModel()->borderAndPaddingLogicalHeight();
    }

    bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, const InlineFlowBox* inlineFlowBox = nullptr) const
    {
        return toBoxModel()->boxShadowShouldBeAppliedToBackground(bleedAvoidance, inlineFlowBox);
    }

    LayoutSize offsetForInFlowPosition() const
    {
        return toBoxModel()->offsetForInFlowPosition();
    }

private:
    LayoutBoxModelObject* toBoxModel() { return toLayoutBoxModelObject(layoutObject()); }
    const LayoutBoxModelObject* toBoxModel() const { return toLayoutBoxModelObject(layoutObject()); }
};

inline LineLayoutBoxModel LineLayoutItem::enclosingBoxModelObject() const
{
    return LineLayoutBoxModel(layoutObject()->enclosingBoxModelObject());
}

} // namespace blink

#endif // LineLayoutBoxModel_h
