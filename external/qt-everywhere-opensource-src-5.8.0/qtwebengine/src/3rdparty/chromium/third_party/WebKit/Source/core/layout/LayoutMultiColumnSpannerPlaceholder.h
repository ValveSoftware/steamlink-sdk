// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutMultiColumnSpannerPlaceholder_h
#define LayoutMultiColumnSpannerPlaceholder_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

// Placeholder layoutObject for column-span:all elements. The column-span:all layoutObject itself is a
// descendant of the flow thread, but due to its out-of-flow nature, we need something on the
// outside to take care of its positioning and sizing. LayoutMultiColumnSpannerPlaceholder objects
// are siblings of LayoutMultiColumnSet objects, i.e. direct children of the multicol container.
class LayoutMultiColumnSpannerPlaceholder final : public LayoutBox {
public:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutMultiColumnSpannerPlaceholder || LayoutBox::isOfType(type); }

    static LayoutMultiColumnSpannerPlaceholder* createAnonymous(const ComputedStyle& parentStyle, LayoutBox&);

    LayoutMultiColumnFlowThread* flowThread() const { return toLayoutBlockFlow(parent())->multiColumnFlowThread(); }

    LayoutBox* layoutObjectInFlowThread() const { return m_layoutObjectInFlowThread; }
    void markForLayoutIfObjectInFlowThreadNeedsLayout()
    {
        if (!m_layoutObjectInFlowThread->needsLayout())
            return;
        // The containing block of a spanner is the multicol container (our parent here), but the
        // spanner is laid out via its spanner set (us), so we need to make sure that we enter it.
        setChildNeedsLayout(MarkOnlyThis);
    }

    void layoutObjectInFlowThreadStyleDidChange(const ComputedStyle* oldStyle);
    void updateMarginProperties();

    const char* name() const override { return "LayoutMultiColumnSpannerPlaceholder"; }

protected:
    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;
    bool needsPreferredWidthsRecalculation() const override;
    LayoutUnit minPreferredLogicalWidth() const override;
    LayoutUnit maxPreferredLogicalWidth() const override;
    void layout() override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    void invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState&) override;
    void paint(const PaintInfo&, const LayoutPoint& paintOffset) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

private:
    LayoutMultiColumnSpannerPlaceholder(LayoutBox*);

    LayoutBox* m_layoutObjectInFlowThread; // The actual column-span:all layoutObject inside the flow thread.
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMultiColumnSpannerPlaceholder, isLayoutMultiColumnSpannerPlaceholder());

} // namespace blink

#endif
