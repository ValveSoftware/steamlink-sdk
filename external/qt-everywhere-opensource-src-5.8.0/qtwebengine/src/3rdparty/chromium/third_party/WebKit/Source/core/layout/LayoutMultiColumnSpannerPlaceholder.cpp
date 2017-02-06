// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"

namespace blink {

static void copyMarginProperties(ComputedStyle& placeholderStyle, const ComputedStyle& spannerStyle)
{
    // We really only need the block direction margins, but there are no setters for that in
    // ComputedStyle. Just copy all margin sides. The inline ones don't matter anyway.
    placeholderStyle.setMarginLeft(spannerStyle.marginLeft());
    placeholderStyle.setMarginRight(spannerStyle.marginRight());
    placeholderStyle.setMarginTop(spannerStyle.marginTop());
    placeholderStyle.setMarginBottom(spannerStyle.marginBottom());
}

LayoutMultiColumnSpannerPlaceholder* LayoutMultiColumnSpannerPlaceholder::createAnonymous(const ComputedStyle& parentStyle, LayoutBox& layoutObjectInFlowThread)
{
    LayoutMultiColumnSpannerPlaceholder* newSpanner = new LayoutMultiColumnSpannerPlaceholder(&layoutObjectInFlowThread);
    Document& document = layoutObjectInFlowThread.document();
    newSpanner->setDocumentForAnonymous(&document);
    RefPtr<ComputedStyle> newStyle = ComputedStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);
    copyMarginProperties(*newStyle, layoutObjectInFlowThread.styleRef());
    newSpanner->setStyle(newStyle);
    return newSpanner;
}

LayoutMultiColumnSpannerPlaceholder::LayoutMultiColumnSpannerPlaceholder(LayoutBox* layoutObjectInFlowThread)
    : LayoutBox(nullptr)
    , m_layoutObjectInFlowThread(layoutObjectInFlowThread)
{
}

void LayoutMultiColumnSpannerPlaceholder::layoutObjectInFlowThreadStyleDidChange(const ComputedStyle* oldStyle)
{
    LayoutBox* objectInFlowThread = m_layoutObjectInFlowThread;
    if (flowThread()->removeSpannerPlaceholderIfNoLongerValid(objectInFlowThread)) {
        // No longer a valid spanner, due to style changes. |this| is now dead.
        if (objectInFlowThread->style()->hasOutOfFlowPosition() && !oldStyle->hasOutOfFlowPosition()) {
            // We went from being a spanner to being out-of-flow positioned. When an object becomes
            // out-of-flow positioned, we need to lay out its parent, since that's where the
            // now-out-of-flow object gets added to the right containing block for out-of-flow
            // positioned objects. Since neither a spanner nor an out-of-flow object is guaranteed
            // to have this parent in its containing block chain, we need to mark it here, or we
            // risk that the object isn't laid out.
            objectInFlowThread->parent()->setNeedsLayout(LayoutInvalidationReason::ColumnsChanged);
        }
        return;
    }
    updateMarginProperties();
}

void LayoutMultiColumnSpannerPlaceholder::updateMarginProperties()
{
    RefPtr<ComputedStyle> newStyle = ComputedStyle::clone(styleRef());
    copyMarginProperties(*newStyle, m_layoutObjectInFlowThread->styleRef());
    setStyle(newStyle);
}

void LayoutMultiColumnSpannerPlaceholder::insertedIntoTree()
{
    LayoutBox::insertedIntoTree();
    // The object may previously have been laid out as a non-spanner, but since it's a spanner now,
    // it needs to be relaid out.
    m_layoutObjectInFlowThread->setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::ColumnsChanged);
}

void LayoutMultiColumnSpannerPlaceholder::willBeRemovedFromTree()
{
    if (m_layoutObjectInFlowThread) {
        LayoutBox* exSpanner = m_layoutObjectInFlowThread;
        m_layoutObjectInFlowThread->clearSpannerPlaceholder();
        // Even if the placeholder is going away, the object in the flow thread might live on. Since
        // it's not a spanner anymore, it needs to be relaid out.
        exSpanner->setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::ColumnsChanged);
    }
    LayoutBox::willBeRemovedFromTree();
}

bool LayoutMultiColumnSpannerPlaceholder::needsPreferredWidthsRecalculation() const
{
    return m_layoutObjectInFlowThread->needsPreferredWidthsRecalculation();
}

LayoutUnit LayoutMultiColumnSpannerPlaceholder::minPreferredLogicalWidth() const
{
    return m_layoutObjectInFlowThread->minPreferredLogicalWidth();
}

LayoutUnit LayoutMultiColumnSpannerPlaceholder::maxPreferredLogicalWidth() const
{
    return m_layoutObjectInFlowThread->maxPreferredLogicalWidth();
}

void LayoutMultiColumnSpannerPlaceholder::layout()
{
    ASSERT(needsLayout());

    // The placeholder, like any other block level object, has its logical top calculated and set
    // before layout. Copy this to the actual column-span:all object before laying it out, so that
    // it gets paginated correctly, in case we have an enclosing fragmentation context.
    m_layoutObjectInFlowThread->setLogicalTop(logicalTop());

    // Lay out the actual column-span:all element.
    m_layoutObjectInFlowThread->layoutIfNeeded();

    // The spanner has now been laid out, so its height is known. Time to update the placeholder's
    // height as well, so that we take up the correct amount of space in the multicol container.
    updateLogicalHeight();

    // Take the overflow from the spanner, so that it gets
    // propagated to the multicol container and beyond.
    m_overflow.reset();
    addContentsVisualOverflow(m_layoutObjectInFlowThread->visualOverflowRect());
    addLayoutOverflow(m_layoutObjectInFlowThread->layoutOverflowRect());

    clearNeedsLayout();
}

void LayoutMultiColumnSpannerPlaceholder::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = m_layoutObjectInFlowThread->logicalHeight();
    computedValues.m_position = logicalTop;
    computedValues.m_margins.m_before = marginBefore();
    computedValues.m_margins.m_after = marginAfter();
}

void LayoutMultiColumnSpannerPlaceholder::invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState& childPaintInvalidationState)
{
    m_layoutObjectInFlowThread->invalidateTreeIfNeeded(childPaintInvalidationState);
    LayoutBox::invalidatePaintOfSubtreesIfNeeded(childPaintInvalidationState);
}

void LayoutMultiColumnSpannerPlaceholder::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    if (!m_layoutObjectInFlowThread->hasSelfPaintingLayer())
        m_layoutObjectInFlowThread->paint(paintInfo, paintOffset);
}

bool LayoutMultiColumnSpannerPlaceholder::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    return !m_layoutObjectInFlowThread->hasSelfPaintingLayer() && m_layoutObjectInFlowThread->nodeAtPoint(result, locationInContainer, accumulatedOffset, action);
}

} // namespace blink
