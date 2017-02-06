// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockPainter.h"

#include "core/editing/DragCaretController.h"
#include "core/editing/FrameSelection.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/page/Page.h"
#include "core/paint/BlockFlowPainter.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "wtf/Optional.h"

namespace blink {

void BlockPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutBlock.location();
    if (!intersectsPaintRect(paintInfo, adjustedPaintOffset))
        return;

    PaintInfo localPaintInfo(paintInfo);
    PaintPhase originalPhase = localPaintInfo.phase;

    // There are some cases where not all clipped visual overflow is accounted for.
    // FIXME: reduce the number of such cases.
    ContentsClipBehavior contentsClipBehavior = ForceContentsClip;
    if (m_layoutBlock.hasOverflowClip() && !m_layoutBlock.hasControlClip() && !m_layoutBlock.hasCaret())
        contentsClipBehavior = SkipContentsClipIfPossible;

    if (originalPhase == PaintPhaseOutline) {
        localPaintInfo.phase = PaintPhaseDescendantOutlinesOnly;
    } else if (shouldPaintSelfBlockBackground(originalPhase)) {
        localPaintInfo.phase = PaintPhaseSelfBlockBackgroundOnly;
        m_layoutBlock.paintObject(localPaintInfo, adjustedPaintOffset);
        if (shouldPaintDescendantBlockBackgrounds(originalPhase))
            localPaintInfo.phase = PaintPhaseDescendantBlockBackgroundsOnly;
    }

    if (originalPhase != PaintPhaseSelfBlockBackgroundOnly && originalPhase != PaintPhaseSelfOutlineOnly) {
        BoxClipper boxClipper(m_layoutBlock, localPaintInfo, adjustedPaintOffset, contentsClipBehavior);
        m_layoutBlock.paintObject(localPaintInfo, adjustedPaintOffset);
    }

    if (shouldPaintSelfOutline(originalPhase)) {
        localPaintInfo.phase = PaintPhaseSelfOutlineOnly;
        m_layoutBlock.paintObject(localPaintInfo, adjustedPaintOffset);
    }

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    localPaintInfo.phase = originalPhase;
    paintOverflowControlsIfNeeded(localPaintInfo, adjustedPaintOffset);
}

void BlockPainter::paintOverflowControlsIfNeeded(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_layoutBlock.hasOverflowClip()
        && m_layoutBlock.style()->visibility() == VISIBLE
        && shouldPaintSelfBlockBackground(paintInfo.phase)
        && !paintInfo.paintRootBackgroundOnly()) {
        Optional<ClipRecorder> clipRecorder;
        if (!m_layoutBlock.layer()->isSelfPaintingLayer()) {
            LayoutRect clipRect = m_layoutBlock.borderBoxRect();
            clipRect.moveBy(paintOffset);
            clipRecorder.emplace(paintInfo.context, m_layoutBlock, DisplayItem::ClipScrollbarsToBoxBounds, pixelSnappedIntRect(clipRect));
        }
        ScrollableAreaPainter(*m_layoutBlock.layer()->getScrollableArea()).paintOverflowControls(paintInfo.context, roundedIntPoint(paintOffset), paintInfo.cullRect(), false /* paintingOverlayControls */);
    }
}

void BlockPainter::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    for (LayoutBox* child = m_layoutBlock.firstChildBox(); child; child = child->nextSiblingBox())
        paintChild(*child, paintInfo, paintOffset);
}

void BlockPainter::paintChild(const LayoutBox& child, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint childPoint = m_layoutBlock.flipForWritingModeForChild(&child, paintOffset);
    if (!child.hasSelfPaintingLayer() && !child.isFloating() && !child.isColumnSpanAll())
        child.paint(paintInfo, childPoint);
}

void BlockPainter::paintChildrenOfFlexibleBox(const LayoutFlexibleBox& layoutFlexibleBox, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    for (const LayoutBox* child = layoutFlexibleBox.orderIterator().first(); child; child = layoutFlexibleBox.orderIterator().next())
        BlockPainter(layoutFlexibleBox).paintAllChildPhasesAtomically(*child, paintInfo, paintOffset);
}

void BlockPainter::paintAllChildPhasesAtomically(const LayoutBox& child, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint childPoint = m_layoutBlock.flipForWritingModeForChild(&child, paintOffset);
    if (!child.hasSelfPaintingLayer() && !child.isFloating())
        ObjectPainter(child).paintAllPhasesAtomically(paintInfo, childPoint);
}

void BlockPainter::paintInlineBox(const InlineBox& inlineBox, const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    // Text clips are painted only for the direct inline children of the object that has a text clip style on it, not block children.
    ASSERT(paintInfo.phase != PaintPhaseTextClip);

    LayoutPoint childPoint = paintOffset;
    if (inlineBox.parent()->getLineLayoutItem().style()->isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = LineLayoutAPIShim::layoutObjectFrom(inlineBox.getLineLayoutItem())->containingBlock()->flipForWritingModeForChild(toLayoutBox(LineLayoutAPIShim::layoutObjectFrom(inlineBox.getLineLayoutItem())), childPoint);

    ObjectPainter(*LineLayoutAPIShim::constLayoutObjectFrom(inlineBox.getLineLayoutItem())).paintAllPhasesAtomically(paintInfo, childPoint);
}

void BlockPainter::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    const PaintPhase paintPhase = paintInfo.phase;

    if (shouldPaintSelfBlockBackground(paintPhase)) {
        if (m_layoutBlock.style()->visibility() == VISIBLE && m_layoutBlock.hasBoxDecorationBackground())
            m_layoutBlock.paintBoxDecorationBackground(paintInfo, paintOffset);
        // We're done. We don't bother painting any children.
        if (paintPhase == PaintPhaseSelfBlockBackgroundOnly)
            return;
    }

    if (paintInfo.paintRootBackgroundOnly())
        return;

    if (paintPhase == PaintPhaseMask && m_layoutBlock.style()->visibility() == VISIBLE) {
        m_layoutBlock.paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhaseClippingMask && m_layoutBlock.style()->visibility() == VISIBLE) {
        BoxPainter(m_layoutBlock).paintClippingMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhaseForeground && paintInfo.isPrinting())
        ObjectPainter(m_layoutBlock).addPDFURLRectIfNeeded(paintInfo, paintOffset);

    if (paintPhase != PaintPhaseSelfOutlineOnly) {
        Optional<ScopedPaintChunkProperties> m_scopedScrollProperty;
        Optional<ScrollRecorder> scrollRecorder;
        Optional<PaintInfo> scrolledPaintInfo;
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            const auto* objectProperties = m_layoutBlock.objectPaintProperties();
            if (auto* scrollTranslation = objectProperties ? objectProperties->scrollTranslation() : nullptr) {
                PaintChunkProperties properties(paintInfo.context.getPaintController().currentPaintChunkProperties());
                properties.transform = scrollTranslation;
                m_scopedScrollProperty.emplace(paintInfo.context.getPaintController(), properties);
                scrolledPaintInfo.emplace(paintInfo);
                scrolledPaintInfo->updateCullRect(scrollTranslation->matrix().toAffineTransform());
            }
        } else if (m_layoutBlock.hasOverflowClip()) {
            IntSize scrollOffset = m_layoutBlock.scrolledContentOffset();
            if (m_layoutBlock.layer()->scrollsOverflow() || !scrollOffset.isZero()) {
                scrollRecorder.emplace(paintInfo.context, m_layoutBlock, paintPhase, scrollOffset);
                scrolledPaintInfo.emplace(paintInfo);
                AffineTransform transform;
                transform.translate(-scrollOffset.width(), -scrollOffset.height());
                scrolledPaintInfo->updateCullRect(transform);
            }
        }

        const PaintInfo& contentsPaintInfo = scrolledPaintInfo ? *scrolledPaintInfo : paintInfo;

        if (m_layoutBlock.isLayoutBlockFlow()) {
            BlockFlowPainter blockFlowPainter(toLayoutBlockFlow(m_layoutBlock));
            blockFlowPainter.paintContents(contentsPaintInfo, paintOffset);
            if (paintPhase == PaintPhaseFloat || paintPhase == PaintPhaseSelection || paintPhase == PaintPhaseTextClip)
                blockFlowPainter.paintFloats(contentsPaintInfo, paintOffset);
        } else {
            paintContents(contentsPaintInfo, paintOffset);
        }
    }

    if (shouldPaintSelfOutline(paintPhase))
        ObjectPainter(m_layoutBlock).paintOutline(paintInfo, paintOffset);

    // If the caret's node's layout object's containing block is this block, and the paint action is PaintPhaseForeground,
    // then paint the caret.
    if (paintPhase == PaintPhaseForeground && m_layoutBlock.hasCaret())
        paintCarets(paintInfo, paintOffset);
}

void BlockPainter::paintCarets(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LocalFrame* frame = m_layoutBlock.frame();

    if (m_layoutBlock.hasCursorCaret())
        frame->selection().paintCaret(paintInfo.context, paintOffset);

    if (m_layoutBlock.hasDragCaret())
        frame->page()->dragCaretController().paintDragCaret(frame, paintInfo.context, paintOffset);
}

bool BlockPainter::intersectsPaintRect(const PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset) const
{
    LayoutRect overflowRect;
    if (paintInfo.isPrinting() && m_layoutBlock.isAnonymousBlock() && m_layoutBlock.childrenInline()) {
        // For case <a href="..."><div>...</div></a>, when m_layoutBlock is the anonymous container
        // of <a>, the anonymous container's visual overflow is empty, but we need to continue
        // painting to output <a>'s PDF URL rect which covers the continuations, as if we included
        // <a>'s PDF URL rect into m_layoutBlock's visual overflow.
        Vector<LayoutRect> rects;
        m_layoutBlock.addElementVisualOverflowRects(rects, LayoutPoint());
        overflowRect = unionRect(rects);
    }
    overflowRect.unite(m_layoutBlock.visualOverflowRect());

    if (m_layoutBlock.hasOverflowModel() && m_layoutBlock.usesCompositedScrolling()) {
        overflowRect.unite(m_layoutBlock.layoutOverflowRect());
        overflowRect.move(-m_layoutBlock.scrolledContentOffset());
    }
    m_layoutBlock.flipForWritingMode(overflowRect);
    overflowRect.moveBy(adjustedPaintOffset);
    return paintInfo.cullRect().intersectsCullRect(overflowRect);
}

void BlockPainter::paintContents(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    DCHECK(!m_layoutBlock.childrenInline());
    PaintInfo paintInfoForDescendants = paintInfo.forDescendants();
    m_layoutBlock.paintChildren(paintInfoForDescendants, paintOffset);
}

} // namespace blink
