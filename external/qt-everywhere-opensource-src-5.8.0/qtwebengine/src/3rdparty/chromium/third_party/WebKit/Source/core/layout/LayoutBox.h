/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef LayoutBox_h
#define LayoutBox_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/OverflowModel.h"
#include "core/layout/ScrollEnums.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class LayoutBlockFlow;
class LayoutMultiColumnSpannerPlaceholder;
class ShapeOutsideInfo;

struct PaintInfo;

enum SizeType { MainOrPreferredSize, MinSize, MaxSize };
enum AvailableLogicalHeightType { ExcludeMarginBorderPadding, IncludeMarginBorderPadding };
// When painting, overlay scrollbars do not take up space and should not affect
// clipping behavior. During hit testing, overlay scrollbars behave like regular
// scrollbars and should change how hit testing is clipped.
enum MarginDirection { BlockDirection, InlineDirection };

enum ShouldComputePreferred { ComputeActual, ComputePreferred };

enum ApplyOverflowClipFlag {
    ApplyOverflowClip,
    ApplyNonScrollOverflowClip
};

using SnapAreaSet = HashSet<const LayoutBox*>;

struct LayoutBoxRareData {
    WTF_MAKE_NONCOPYABLE(LayoutBoxRareData); USING_FAST_MALLOC(LayoutBoxRareData);
public:
    LayoutBoxRareData()
        : m_spannerPlaceholder(nullptr)
        , m_overrideLogicalContentHeight(-1)
        , m_overrideLogicalContentWidth(-1)
        , m_previousBorderBoxSize(LayoutUnit(-1), LayoutUnit(-1))
        , m_percentHeightContainer(nullptr)
        , m_snapContainer(nullptr)
        , m_snapAreas(nullptr)
    {
    }

    // For spanners, the spanner placeholder that lays us out within the multicol container.
    LayoutMultiColumnSpannerPlaceholder* m_spannerPlaceholder;

    LayoutUnit m_overrideLogicalContentHeight;
    LayoutUnit m_overrideLogicalContentWidth;

    // Set by LayoutBox::savePreviousBoxSizesIfNeeded().
    LayoutSize m_previousBorderBoxSize;
    LayoutRect m_previousContentBoxRect;
    LayoutRect m_previousLayoutOverflowRect;

    LayoutUnit m_pageLogicalOffset;

    LayoutUnit m_paginationStrut;

    LayoutBlock* m_percentHeightContainer;
    // For snap area, the owning snap container.
    LayoutBox* m_snapContainer;
    // For snap container, the descendant snap areas that contribute snap
    // points.
    std::unique_ptr<SnapAreaSet> m_snapAreas;

    SnapAreaSet& ensureSnapAreas()
    {
        if (!m_snapAreas)
            m_snapAreas = wrapUnique(new SnapAreaSet);

        return *m_snapAreas;
    }
};

// LayoutBox implements the full CSS box model.
//
// LayoutBoxModelObject only introduces some abstractions for LayoutInline and
// LayoutBox. The logic for the model is in LayoutBox, e.g. the storage for the
// rectangle and offset forming the CSS box (m_frameRect) and the getters for
// the different boxes.
//
// LayoutBox is also the uppermost class to support scrollbars, however the
// logic is delegated to PaintLayerScrollableArea.
// Per the CSS specification, scrollbars should "be inserted between the inner
// border edge and the outer padding edge".
// (see http://www.w3.org/TR/CSS21/visufx.html#overflow)
// Also the scrollbar width / height are removed from the content box. Taking
// the following example:
//
// <!DOCTYPE html>
// <style>
// ::-webkit-scrollbar {
//     /* Force non-overlay scrollbars */
//     width: 10px;
//     height: 20px;
// }
// </style>
// <div style="overflow:scroll; width: 100px; height: 100px">
//
// The <div>'s content box is not 100x100 as specified in the style but 90x80 as
// we remove the scrollbars from the box.
//
// The presence of scrollbars is determined by the 'overflow' property and can
// be conditioned on having layout overflow (see OverflowModel for more details
// on how we track overflow).
//
// There are 2 types of scrollbars:
// - non-overlay scrollbars take space from the content box.
// - overlay scrollbars don't and just overlay hang off from the border box,
//   potentially overlapping with the padding box's content.
// For more details on scrollbars, see PaintLayerScrollableArea.
//
//
// ***** THE BOX MODEL *****
// The CSS box model is based on a series of nested boxes:
// http://www.w3.org/TR/CSS21/box.html
//
//       |----------------------------------------------------|
//       |                                                    |
//       |                   margin-top                       |
//       |                                                    |
//       |     |-----------------------------------------|    |
//       |     |                                         |    |
//       |     |             border-top                  |    |
//       |     |                                         |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                          |    |    |    |
//       |     |    |       padding-top        |####|    |    |
//       |     |    |                          |####|    |    |
//       |     |    |    |----------------|    |####|    |    |
//       |     |    |    |                |    |    |    |    |
//       | ML  | BL | PL |  content box   | PR | SW | BR | MR |
//       |     |    |    |                |    |    |    |    |
//       |     |    |    |----------------|    |    |    |    |
//       |     |    |                          |    |    |    |
//       |     |    |      padding-bottom      |    |    |    |
//       |     |    |--------------------------|----|    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |     scrollbar height ####| SC |    |    |
//       |     |    |                      ####|    |    |    |
//       |     |    |-------------------------------|    |    |
//       |     |                                         |    |
//       |     |           border-bottom                 |    |
//       |     |                                         |    |
//       |     |-----------------------------------------|    |
//       |                                                    |
//       |                 margin-bottom                      |
//       |                                                    |
//       |----------------------------------------------------|
//
// BL = border-left
// BR = border-right
// ML = margin-left
// MR = margin-right
// PL = padding-left
// PR = padding-right
// SC = scroll corner (contains UI for resizing (see the 'resize' property)
// SW = scrollbar width
//
// Those are just the boxes from the CSS model. Extra boxes are tracked by Blink
// (e.g. the overflows). Thus it is paramount to know which box a function is
// manipulating. Also of critical importance is the coordinate system used (see
// the COORDINATE SYSTEMS section in LayoutBoxModelObject).
class CORE_EXPORT LayoutBox : public LayoutBoxModelObject {
public:
    explicit LayoutBox(ContainerNode*);

    PaintLayerType layerTypeRequired() const override;

    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const override;

    virtual bool backgroundShouldAlwaysBeClipped() const { return false; }

    // Use this with caution! No type checking is done!
    LayoutBox* firstChildBox() const;
    LayoutBox* firstInFlowChildBox() const;
    LayoutBox* lastChildBox() const;

    int pixelSnappedWidth() const { return m_frameRect.pixelSnappedWidth(); }
    int pixelSnappedHeight() const { return m_frameRect.pixelSnappedHeight(); }

    void setX(LayoutUnit x)
    {
        if (x == m_frameRect.x())
            return;
        m_frameRect.setX(x);
        frameRectChanged();
    }
    void setY(LayoutUnit y)
    {
        if (y == m_frameRect.y())
            return;
        m_frameRect.setY(y);
        frameRectChanged();
    }
    void setWidth(LayoutUnit width)
    {
        if (width == m_frameRect.width())
            return;
        m_frameRect.setWidth(width);
        frameRectChanged();
    }
    void setHeight(LayoutUnit height)
    {
        if (height == m_frameRect.height())
            return;
        m_frameRect.setHeight(height);
        frameRectChanged();
    }

    LayoutUnit logicalLeft() const { return style()->isHorizontalWritingMode() ? m_frameRect.x() : m_frameRect.y(); }
    LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }
    LayoutUnit logicalTop() const { return style()->isHorizontalWritingMode() ? m_frameRect.y() : m_frameRect.x(); }
    LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }
    LayoutUnit logicalWidth() const { return style()->isHorizontalWritingMode() ? m_frameRect.width() : m_frameRect.height(); }
    LayoutUnit logicalHeight() const { return style()->isHorizontalWritingMode() ? m_frameRect.height() : m_frameRect.width(); }

    LayoutUnit constrainLogicalWidthByMinMax(LayoutUnit, LayoutUnit, LayoutBlock*) const;
    LayoutUnit constrainLogicalHeightByMinMax(LayoutUnit logicalHeight, LayoutUnit intrinsicContentHeight) const;
    LayoutUnit constrainContentBoxLogicalHeightByMinMax(LayoutUnit logicalHeight, LayoutUnit intrinsicContentHeight) const;

    int pixelSnappedLogicalHeight() const { return style()->isHorizontalWritingMode() ? pixelSnappedHeight() : pixelSnappedWidth(); }
    int pixelSnappedLogicalWidth() const { return style()->isHorizontalWritingMode() ? pixelSnappedWidth() : pixelSnappedHeight(); }

    LayoutUnit minimumLogicalHeightForEmptyLine() const
    {
        return borderAndPaddingLogicalHeight() + scrollbarLogicalHeight()
            + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    }

    void setLogicalLeft(LayoutUnit left)
    {
        if (style()->isHorizontalWritingMode())
            setX(left);
        else
            setY(left);
    }
    void setLogicalTop(LayoutUnit top)
    {
        if (style()->isHorizontalWritingMode())
            setY(top);
        else
            setX(top);
    }
    void setLogicalLocation(const LayoutPoint& location)
    {
        if (style()->isHorizontalWritingMode())
            setLocation(location);
        else
            setLocation(location.transposedPoint());
    }
    void setLogicalWidth(LayoutUnit size)
    {
        if (style()->isHorizontalWritingMode())
            setWidth(size);
        else
            setHeight(size);
    }
    void setLogicalHeight(LayoutUnit size)
    {
        if (style()->isHorizontalWritingMode())
            setHeight(size);
        else
            setWidth(size);
    }

    LayoutPoint location() const { return m_frameRect.location(); }
    LayoutSize locationOffset() const { return LayoutSize(m_frameRect.x(), m_frameRect.y()); }
    LayoutSize size() const { return m_frameRect.size(); }
    IntSize pixelSnappedSize() const { return m_frameRect.pixelSnappedSize(); }

    void setLocation(const LayoutPoint& location)
    {
        if (location == m_frameRect.location())
            return;
        m_frameRect.setLocation(location);
        frameRectChanged();
    }

    // FIXME: Currently scrollbars are using int geometry and positioned based on
    // pixelSnappedBorderBoxRect whose size may change when location changes because of
    // pixel snapping. This function is used to change location of the LayoutBox outside
    // of LayoutBox::layout(). Will remove when we use LayoutUnits for scrollbars.
    void setLocationAndUpdateOverflowControlsIfNeeded(const LayoutPoint&);

    void setSize(const LayoutSize& size)
    {
        if (size == m_frameRect.size())
            return;
        m_frameRect.setSize(size);
        frameRectChanged();
    }
    void move(LayoutUnit dx, LayoutUnit dy)
    {
        if (!dx && !dy)
            return;
        m_frameRect.move(dx, dy);
        frameRectChanged();
    }

    // This function is in the container's coordinate system, meaning
    // that it includes the logical top/left offset and the
    // inline-start/block-start margins.
    LayoutRect frameRect() const { return m_frameRect; }
    void setFrameRect(const LayoutRect& rect)
    {
        if (rect == m_frameRect)
            return;
        m_frameRect = rect;
        frameRectChanged();
    }

    // Note that those functions have their origin at this box's CSS border box.
    // As such their location doesn't account for 'top'/'left'.
    LayoutRect borderBoxRect() const { return LayoutRect(LayoutPoint(), size()); }
    LayoutRect paddingBoxRect() const { return LayoutRect(LayoutUnit(borderLeft()), LayoutUnit(borderTop()), clientWidth(), clientHeight()); }
    IntRect pixelSnappedBorderBoxRect() const { return IntRect(IntPoint(), m_frameRect.pixelSnappedSize()); }
    IntRect borderBoundingBox() const final { return pixelSnappedBorderBoxRect(); }

    // The content area of the box (excludes padding - and intrinsic padding for table cells, etc... - and border).
    LayoutRect contentBoxRect() const { return LayoutRect(borderLeft() + paddingLeft(), borderTop() + paddingTop(), contentWidth(), contentHeight()); }
    LayoutSize contentBoxOffset() const { return LayoutSize(borderLeft() + paddingLeft(), borderTop() + paddingTop()); }
    // The content box in absolute coords. Ignores transforms.
    IntRect absoluteContentBox() const;
    // The offset of the content box in absolute coords, ignoring transforms.
    IntSize absoluteContentBoxOffset() const;
    // The content box converted to absolute coords (taking transforms into account).
    FloatQuad absoluteContentQuad() const;

    // This returns the content area of the box (excluding padding and border). The only difference with contentBoxRect is that computedCSSContentBoxRect
    // does include the intrinsic padding in the content box as this is what some callers expect (like getComputedStyle).
    LayoutRect computedCSSContentBoxRect() const { return LayoutRect(borderLeft() + computedCSSPaddingLeft(), borderTop() + computedCSSPaddingTop(), clientWidth() - computedCSSPaddingLeft() - computedCSSPaddingRight(), clientHeight() - computedCSSPaddingTop() - computedCSSPaddingBottom()); }

    void addOutlineRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const override;

    // Use this with caution! No type checking is done!
    LayoutBox* previousSiblingBox() const;
    LayoutBox* previousInFlowSiblingBox() const;
    LayoutBox* nextSiblingBox() const;
    LayoutBox* nextInFlowSiblingBox() const;
    LayoutBox* parentBox() const;

    // Return the previous sibling column set or spanner placeholder. Only to be used on multicol container children.
    LayoutBox* previousSiblingMultiColumnBox() const;
    // Return the next sibling column set or spanner placeholder. Only to be used on multicol container children.
    LayoutBox* nextSiblingMultiColumnBox() const;

    bool canResize() const;

    // Visual and layout overflow are in the coordinate space of the box.  This means that they
    // aren't purely physical directions. For horizontal-tb and vertical-lr they will match physical
    // directions, but for vertical-rl, the left/right are flipped when compared to their physical
    // counterparts.  For example minX is on the left in vertical-lr, but it is on the right in
    // vertical-rl.
    LayoutRect noOverflowRect() const;
    LayoutRect layoutOverflowRect() const { return m_overflow ? m_overflow->layoutOverflowRect() : noOverflowRect(); }
    IntRect pixelSnappedLayoutOverflowRect() const { return pixelSnappedIntRect(layoutOverflowRect()); }
    LayoutSize maxLayoutOverflow() const { return LayoutSize(layoutOverflowRect().maxX(), layoutOverflowRect().maxY()); }
    LayoutUnit logicalLeftLayoutOverflow() const { return style()->isHorizontalWritingMode() ? layoutOverflowRect().x() : layoutOverflowRect().y(); }
    LayoutUnit logicalRightLayoutOverflow() const { return style()->isHorizontalWritingMode() ? layoutOverflowRect().maxX() : layoutOverflowRect().maxY(); }

    LayoutRect visualOverflowRect() const override;
    LayoutUnit logicalLeftVisualOverflow() const { return style()->isHorizontalWritingMode() ? visualOverflowRect().x() : visualOverflowRect().y(); }
    LayoutUnit logicalRightVisualOverflow() const { return style()->isHorizontalWritingMode() ? visualOverflowRect().maxX() : visualOverflowRect().maxY(); }

    LayoutRect selfVisualOverflowRect() const { return m_overflow ? m_overflow->selfVisualOverflowRect() : borderBoxRect(); }
    LayoutRect contentsVisualOverflowRect() const { return m_overflow ? m_overflow->contentsVisualOverflowRect() : LayoutRect(); }

    // These methods don't mean the box *actually* has top/left overflow.  They mean that
    // *if* the box overflows, it will overflow to the top/left rather than the bottom/right.
    // This happens when child content is laid out right-to-left (e.g. direction:rtl) or
    // or bottom-to-top (e.g. direction:rtl writing-mode:vertical-rl).
    virtual bool hasTopOverflow() const;
    virtual bool hasLeftOverflow() const;

    void addLayoutOverflow(const LayoutRect&);
    void addSelfVisualOverflow(const LayoutRect&);
    void addContentsVisualOverflow(const LayoutRect&);

    void addVisualEffectOverflow();
    LayoutRectOutsets computeVisualEffectOverflowOutsets() const;
    void addOverflowFromChild(LayoutBox* child) { addOverflowFromChild(child, child->locationOffset()); }
    void addOverflowFromChild(LayoutBox* child, const LayoutSize& delta);
    void clearLayoutOverflow();
    void clearAllOverflows() { m_overflow.reset(); }

    void updateLayerTransformAfterLayout();

    LayoutUnit contentWidth() const { return clientWidth() - paddingLeft() - paddingRight(); }
    LayoutUnit contentHeight() const { return clientHeight() - paddingTop() - paddingBottom(); }
    LayoutSize contentSize() const { return LayoutSize(contentWidth(), contentHeight()); }
    LayoutUnit contentLogicalWidth() const { return style()->isHorizontalWritingMode() ? contentWidth() : contentHeight(); }
    LayoutUnit contentLogicalHeight() const { return style()->isHorizontalWritingMode() ? contentHeight() : contentWidth(); }

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (LayoutFlow)
    // to return the remaining width on a given line (and the height of a single line).
    LayoutUnit offsetWidth() const override { return m_frameRect.width(); }
    LayoutUnit offsetHeight() const override { return m_frameRect.height(); }

    int pixelSnappedOffsetWidth(const Element*) const final;
    int pixelSnappedOffsetHeight(const Element*) const final;

    // More IE extensions.  clientWidth and clientHeight represent the interior of an object
    // excluding border and scrollbar.  clientLeft/Top are just the borderLeftWidth and borderTopWidth.
    LayoutUnit clientLeft() const { return LayoutUnit(borderLeft() + (shouldPlaceBlockDirectionScrollbarOnLogicalLeft() ? verticalScrollbarWidth() : 0)); }
    LayoutUnit clientTop() const { return LayoutUnit(borderTop()); }
    LayoutUnit clientWidth() const;
    LayoutUnit clientHeight() const;
    LayoutUnit clientLogicalWidth() const { return style()->isHorizontalWritingMode() ? clientWidth() : clientHeight(); }
    LayoutUnit clientLogicalHeight() const { return style()->isHorizontalWritingMode() ? clientHeight() : clientWidth(); }
    LayoutUnit clientLogicalBottom() const { return borderBefore() + clientLogicalHeight(); }
    LayoutRect clientBoxRect() const { return LayoutRect(clientLeft(), clientTop(), clientWidth(), clientHeight()); }

    int pixelSnappedClientWidth() const;
    int pixelSnappedClientHeight() const;

    // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
    // object has overflow:hidden/scroll/auto specified and also has overflow.
    // scrollLeft/Top return the current scroll position.  These methods are virtual so that objects like
    // textareas can scroll shadow content (but pretend that they are the objects that are
    // scrolling).
    virtual LayoutUnit scrollLeft() const;
    virtual LayoutUnit scrollTop() const;
    virtual LayoutUnit scrollWidth() const;
    virtual LayoutUnit scrollHeight() const;
    int pixelSnappedScrollWidth() const;
    int pixelSnappedScrollHeight() const;
    virtual void setScrollLeft(LayoutUnit);
    virtual void setScrollTop(LayoutUnit);

    void scrollToOffset(const DoubleSize&, ScrollBehavior = ScrollBehaviorInstant);
    void scrollByRecursively(const DoubleSize& delta, ScrollOffsetClamping = ScrollOffsetUnclamped);
    // If makeVisibleInVisualViewport is set, the visual viewport will be scrolled
    // if required to make the rect visible.
    void scrollRectToVisible(const LayoutRect&, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType = ProgrammaticScroll, bool makeVisibleInVisualViewport = true);

    LayoutRectOutsets marginBoxOutsets() const override { return m_marginBoxOutsets; }
    LayoutUnit marginTop() const override { return m_marginBoxOutsets.top(); }
    LayoutUnit marginBottom() const override { return m_marginBoxOutsets.bottom(); }
    LayoutUnit marginLeft() const override { return m_marginBoxOutsets.left(); }
    LayoutUnit marginRight() const override { return m_marginBoxOutsets.right(); }
    void setMarginTop(LayoutUnit margin) { m_marginBoxOutsets.setTop(margin); }
    void setMarginBottom(LayoutUnit margin) { m_marginBoxOutsets.setBottom(margin); }
    void setMarginLeft(LayoutUnit margin) { m_marginBoxOutsets.setLeft(margin); }
    void setMarginRight(LayoutUnit margin) { m_marginBoxOutsets.setRight(margin); }

    LayoutUnit marginLogicalLeft() const { return m_marginBoxOutsets.logicalLeft(style()->getWritingMode()); }
    LayoutUnit marginLogicalRight() const { return m_marginBoxOutsets.logicalRight(style()->getWritingMode()); }

    LayoutUnit marginBefore(const ComputedStyle* overrideStyle = nullptr) const final { return m_marginBoxOutsets.before((overrideStyle ? overrideStyle : style())->getWritingMode()); }
    LayoutUnit marginAfter(const ComputedStyle* overrideStyle = nullptr) const final { return m_marginBoxOutsets.after((overrideStyle ? overrideStyle : style())->getWritingMode()); }
    LayoutUnit marginStart(const ComputedStyle* overrideStyle = nullptr) const final
    {
        const ComputedStyle* styleToUse = overrideStyle ? overrideStyle : style();
        return m_marginBoxOutsets.start(styleToUse->getWritingMode(), styleToUse->direction());
    }
    LayoutUnit marginEnd(const ComputedStyle* overrideStyle = nullptr) const final
    {
        const ComputedStyle* styleToUse = overrideStyle ? overrideStyle : style();
        return m_marginBoxOutsets.end(styleToUse->getWritingMode(), styleToUse->direction());
    }
    LayoutUnit marginOver() const final { return m_marginBoxOutsets.over(style()->getWritingMode()); }
    LayoutUnit marginUnder() const final { return m_marginBoxOutsets.under(style()->getWritingMode()); }
    void setMarginBefore(LayoutUnit value, const ComputedStyle* overrideStyle = nullptr) { m_marginBoxOutsets.setBefore((overrideStyle ? overrideStyle : style())->getWritingMode(), value); }
    void setMarginAfter(LayoutUnit value, const ComputedStyle* overrideStyle = nullptr) { m_marginBoxOutsets.setAfter((overrideStyle ? overrideStyle : style())->getWritingMode(), value); }
    void setMarginStart(LayoutUnit value, const ComputedStyle* overrideStyle = nullptr)
    {
        const ComputedStyle* styleToUse = overrideStyle ? overrideStyle : style();
        m_marginBoxOutsets.setStart(styleToUse->getWritingMode(), styleToUse->direction(), value);
    }
    void setMarginEnd(LayoutUnit value, const ComputedStyle* overrideStyle = nullptr)
    {
        const ComputedStyle* styleToUse = overrideStyle ? overrideStyle : style();
        m_marginBoxOutsets.setEnd(styleToUse->getWritingMode(), styleToUse->direction(), value);
    }

    // The following functions are used to implement collapsing margins.
    // All objects know their maximal positive and negative margins.  The
    // formula for computing a collapsed margin is |maxPosMargin| - |maxNegmargin|.
    // For a non-collapsing box, such as a leaf element, this formula will simply return
    // the margin of the element.  Blocks override the maxMarginBefore and maxMarginAfter
    // methods.
    virtual bool isSelfCollapsingBlock() const { return false; }
    virtual LayoutUnit collapsedMarginBefore() const { return marginBefore(); }
    virtual LayoutUnit collapsedMarginAfter() const { return marginAfter(); }
    LayoutRectOutsets collapsedMarginBoxLogicalOutsets() const
    {
        return LayoutRectOutsets(collapsedMarginBefore(), LayoutUnit(), collapsedMarginAfter(), LayoutUnit());
    }

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&) const override;

    int reflectionOffset() const;
    // Given a rect in the object's coordinate space, returns the corresponding rect in the reflection.
    LayoutRect reflectedRect(const LayoutRect&) const;

    void layout() override;
    void paint(const PaintInfo&, const LayoutPoint&) const override;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    LayoutUnit minPreferredLogicalWidth() const override;
    LayoutUnit maxPreferredLogicalWidth() const override;

    // FIXME: We should rename these back to overrideLogicalHeight/Width and have them store
    // the border-box height/width like the regular height/width accessors on LayoutBox.
    // Right now, these are different than contentHeight/contentWidth because they still
    // include the scrollbar height/width.
    LayoutUnit overrideLogicalContentWidth() const;
    LayoutUnit overrideLogicalContentHeight() const;
    bool hasOverrideLogicalContentHeight() const;
    bool hasOverrideLogicalContentWidth() const;
    void setOverrideLogicalContentHeight(LayoutUnit);
    void setOverrideLogicalContentWidth(LayoutUnit);
    void clearOverrideSize();
    void clearOverrideLogicalContentHeight();
    void clearOverrideLogicalContentWidth();

    LayoutUnit overrideContainingBlockContentLogicalWidth() const;
    LayoutUnit overrideContainingBlockContentLogicalHeight() const;
    bool hasOverrideContainingBlockLogicalWidth() const;
    bool hasOverrideContainingBlockLogicalHeight() const;
    void setOverrideContainingBlockContentLogicalWidth(LayoutUnit);
    void setOverrideContainingBlockContentLogicalHeight(LayoutUnit);
    void clearContainingBlockOverrideSize();
    void clearOverrideContainingBlockContentLogicalHeight();

    LayoutUnit extraInlineOffset() const;
    LayoutUnit extraBlockOffset() const;
    void setExtraInlineOffset(LayoutUnit inlineOffest);
    void setExtraBlockOffset(LayoutUnit blockOffest);
    void clearExtraInlineAndBlockOffests();

    LayoutSize offsetFromContainer(const LayoutObject*) const override;

    LayoutUnit adjustBorderBoxLogicalWidthForBoxSizing(float width) const;
    LayoutUnit adjustBorderBoxLogicalHeightForBoxSizing(float height) const;
    LayoutUnit adjustContentBoxLogicalWidthForBoxSizing(float width) const;
    LayoutUnit adjustContentBoxLogicalHeightForBoxSizing(float height) const;

    // ComputedMarginValues holds the actual values for margins. It ignores
    // margin collapsing as they are handled in LayoutBlockFlow.
    // The margins are stored in logical coordinates (see COORDINATE
    // SYSTEMS in LayoutBoxModel) for use during layout.
    struct ComputedMarginValues {
        DISALLOW_NEW();
        ComputedMarginValues() { }

        LayoutUnit m_before;
        LayoutUnit m_after;
        LayoutUnit m_start;
        LayoutUnit m_end;
    };

    // LogicalExtentComputedValues is used both for the
    // block-flow and inline-direction axis.
    struct LogicalExtentComputedValues {
        STACK_ALLOCATED();
        LogicalExtentComputedValues() { }

        // This is the dimension in the measured direction
        // (logical height or logical width).
        LayoutUnit m_extent;

        // This is the offset in the measured direction
        // (logical top or logical left).
        LayoutUnit m_position;

        // |m_margins| represents the margins in the measured direction.
        // Note that ComputedMarginValues has also the margins in
        // the orthogonal direction to have clearer names but they are
        // ignored in the code.
        ComputedMarginValues m_margins;
    };

    // Resolve auto margins in the chosen direction of the containing block so that objects can be pushed to the start, middle or end
    // of the containing block.
    void computeMarginsForDirection(MarginDirection forDirection, const LayoutBlock* containingBlock, LayoutUnit containerWidth, LayoutUnit childWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd, Length marginStartLength, Length marginStartEnd) const;

    // Used to resolve margins in the containing block's block-flow direction.
    void computeAndSetBlockDirectionMargins(const LayoutBlock* containingBlock);

    LayoutUnit offsetFromLogicalTopOfFirstPage() const;

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const { return m_rareData ? m_rareData->m_pageLogicalOffset : LayoutUnit(); }
    void setPageLogicalOffset(LayoutUnit);

    // Specify which page or column to associate with an offset, if said offset is exactly at a page
    // or column boundary.
    enum PageBoundaryRule { AssociateWithFormerPage, AssociateWithLatterPage };
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule) const;

    // Calculate the strut to insert in order fit content of size |contentLogicalHeight|.
    // |strutToNextPage| is the strut to add to |offset| to merely get to the top of the next page
    // or column. This is what will be returned if the content can actually fit there. Otherwise,
    // return the distance to the next fragmentainer that can fit this piece of content.
    virtual LayoutUnit calculatePaginationStrutToFitContent(LayoutUnit offset, LayoutUnit strutToNextPage, LayoutUnit contentLogicalHeight) const;

    void positionLineBox(InlineBox*);
    void moveWithEdgeOfInlineContainerIfNecessary(bool isHorizontal);

    virtual InlineBox* createInlineBox();
    void dirtyLineBoxes(bool fullLayout);

    // For atomic inline elements, this function returns the inline box that contains us.  Enables
    // the atomic inline LayoutObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    InlineBox* inlineBoxWrapper() const { return m_inlineBoxWrapper; }
    void setInlineBoxWrapper(InlineBox*);
    void deleteLineBoxWrapper();

    void setSpannerPlaceholder(LayoutMultiColumnSpannerPlaceholder&);
    void clearSpannerPlaceholder();
    LayoutMultiColumnSpannerPlaceholder* spannerPlaceholder() const final { return m_rareData ? m_rareData->m_spannerPlaceholder : 0; }

    // A pagination strut is the amount of space needed to push an in-flow block-level object (or
    // float) to the logical top of the next page or column. It will be set both for forced breaks
    // (e.g. page-break-before:always) and soft breaks (when there's not enough space in the current
    // page / column for the object). The strut is baked into the logicalTop() of the object, so
    // that logicalTop() - paginationStrut() == the original position in the previous column before
    // deciding to break.
    //
    // Pagination struts are either set in front of a block-level box (here) or before a line
    // (RootInlineBox::paginationStrut()).
    LayoutUnit paginationStrut() const { return m_rareData ? m_rareData->m_paginationStrut : LayoutUnit(); }
    void setPaginationStrut(LayoutUnit);
    void resetPaginationStrut()
    {
        if (m_rareData)
            m_rareData->m_paginationStrut = LayoutUnit();
    }

    // Is the specified break-before or break-after value supported on this object? It needs to be
    // in-flow all the way up to a fragmentation context that supports the specified value.
    bool isBreakBetweenControllable(EBreak) const;

    // Is the specified break-inside value supported on this object? It needs to be contained by a
    // fragmentation context that supports the specified value.
    bool isBreakInsideControllable(EBreak) const;

    virtual EBreak breakAfter() const;
    virtual EBreak breakBefore() const;
    EBreak breakInside() const;

    // Join two adjacent break values specified on break-before and/or break-after. avoid* values
    // win over auto values, and forced break values win over avoid* values. |firstValue| is
    // specified on an element earlier in the flow than |secondValue|. This method is used at class
    // A break points [1], to join the values of the previous break-after and the next
    // break-before, to figure out whether we may, must, or should not, break at that point. It is
    // also used when propagating break-before values from first children and break-after values on
    // last children to their container.
    //
    // [1] https://drafts.csswg.org/css-break/#possible-breaks
    static EBreak joinFragmentainerBreakValues(EBreak firstValue, EBreak secondValue);

    static bool isForcedFragmentainerBreakValue(EBreak);

    EBreak classABreakPointValue(EBreak previousBreakAfterValue) const;

    // Return true if we should insert a break in front of this box. The box needs to start at a
    // valid class A break point in order to allow a forced break. To determine whether or not to
    // break, we also need to know the break-after value of the previous in-flow sibling.
    bool needsForcedBreakBefore(EBreak previousBreakAfterValue) const;

    LayoutRect localOverflowRectForPaintInvalidation() const override;
    bool mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, VisualRectFlags = DefaultVisualRectFlags) const override;
    virtual void invalidatePaintForOverhangingFloats(bool paintAllDescendants);

    LayoutUnit containingBlockLogicalHeightForGetComputedStyle() const;

    LayoutUnit containingBlockLogicalWidthForContent() const override;
    LayoutUnit containingBlockLogicalHeightForContent(AvailableLogicalHeightType) const;

    LayoutUnit containingBlockAvailableLineWidth() const;
    LayoutUnit perpendicularContainingBlockLogicalHeight() const;

    virtual void updateLogicalWidth();
    void updateLogicalHeight();
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const;
    // This function will compute the logical border-box height, without laying out the box. This means that the result
    // is only "correct" when the height is explicitly specified. This function exists so that intrinsic width calculations
    // have a way to deal with children that have orthogonal flows.
    // When there is no explicit height, this function assumes a content height of zero (and returns just border+padding)
    LayoutUnit computeLogicalHeightWithoutLayout() const;

    void computeLogicalWidth(LogicalExtentComputedValues&) const;

    bool stretchesToViewport() const { return document().inQuirksMode() && stretchesToViewportInQuirksMode(); }

    virtual LayoutSize intrinsicSize() const { return LayoutSize(); }
    LayoutUnit intrinsicLogicalWidth() const { return style()->isHorizontalWritingMode() ? intrinsicSize().width() : intrinsicSize().height(); }
    LayoutUnit intrinsicLogicalHeight() const { return style()->isHorizontalWritingMode() ? intrinsicSize().height() : intrinsicSize().width(); }
    virtual LayoutUnit intrinsicContentLogicalHeight() const { return m_intrinsicContentLogicalHeight; }

    // Whether or not the element shrinks to its intrinsic width (rather than filling the width
    // of a containing block).  HTML4 buttons, <select>s, <input>s, legends, and floating/compact elements do this.
    bool sizesLogicalWidthToFitContent(const Length& logicalWidth) const;

    LayoutUnit shrinkLogicalWidthToAvoidFloats(LayoutUnit childMarginStart, LayoutUnit childMarginEnd, const LayoutBlockFlow* cb) const;

    LayoutUnit computeLogicalWidthUsing(SizeType, const Length& logicalWidth, LayoutUnit availableLogicalWidth, const LayoutBlock* containingBlock) const;
    LayoutUnit computeLogicalHeightUsing(SizeType, const Length& height, LayoutUnit intrinsicContentHeight) const;
    LayoutUnit computeContentLogicalHeight(SizeType, const Length& height, LayoutUnit intrinsicContentHeight) const;
    LayoutUnit computeContentAndScrollbarLogicalHeightUsing(SizeType, const Length& height, LayoutUnit intrinsicContentHeight) const;
    LayoutUnit computeReplacedLogicalWidthUsing(SizeType, const Length& width) const;
    LayoutUnit computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, ShouldComputePreferred  = ComputeActual) const;
    LayoutUnit computeReplacedLogicalHeightUsing(SizeType, const Length& height) const;
    LayoutUnit computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const;

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const;
    virtual LayoutUnit computeReplacedLogicalHeight(LayoutUnit estimatedUsedWidth = LayoutUnit()) const;

    bool percentageLogicalHeightIsResolvable() const;
    LayoutUnit computePercentageLogicalHeight(const Length& height) const;

    // Block flows subclass availableWidth/Height to handle multi column layout (shrinking the width/height available to children when laying out.)
    LayoutUnit availableLogicalWidth() const { return contentLogicalWidth(); }
    LayoutUnit availableLogicalHeight(AvailableLogicalHeightType) const;
    LayoutUnit availableLogicalHeightUsing(const Length&, AvailableLogicalHeightType) const;

    // There are a few cases where we need to refer specifically to the available physical width and available physical height.
    // Relative positioning is one of those cases, since left/top offsets are physical.
    LayoutUnit availableWidth() const { return style()->isHorizontalWritingMode() ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding); }
    LayoutUnit availableHeight() const { return style()->isHorizontalWritingMode() ? availableLogicalHeight(IncludeMarginBorderPadding) : availableLogicalWidth(); }

    int verticalScrollbarWidth() const;
    int horizontalScrollbarHeight() const;
    int scrollbarLogicalWidth() const { return style()->isHorizontalWritingMode() ? verticalScrollbarWidth() : horizontalScrollbarHeight(); }
    int scrollbarLogicalHeight() const { return style()->isHorizontalWritingMode() ? horizontalScrollbarHeight() : verticalScrollbarWidth(); }
    virtual ScrollResult scroll(ScrollGranularity, const FloatSize&);
    bool canBeScrolledAndHasScrollableArea() const;
    virtual bool canBeProgramaticallyScrolled() const;
    virtual void autoscroll(const IntPoint&);
    bool canAutoscroll() const;
    IntSize calculateAutoscrollDirection(const IntPoint& pointInRootFrame) const;
    static LayoutBox* findAutoscrollable(LayoutObject*);
    virtual void stopAutoscroll() { }
    virtual void panScroll(const IntPoint&);

    bool hasAutoVerticalScrollbar() const { return hasOverflowClip() && (style()->overflowY() == OverflowAuto || style()->overflowY() == OverflowPagedY || style()->overflowY() == OverflowOverlay); }
    bool hasAutoHorizontalScrollbar() const { return hasOverflowClip() && (style()->overflowX() == OverflowAuto || style()->overflowX() == OverflowOverlay); }
    bool scrollsOverflow() const { return scrollsOverflowX() || scrollsOverflowY(); }
    virtual bool shouldPlaceBlockDirectionScrollbarOnLogicalLeft() const { return style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft(); }

    bool hasScrollableOverflowX() const { return scrollsOverflowX() && pixelSnappedScrollWidth() != pixelSnappedClientWidth(); }
    bool hasScrollableOverflowY() const { return scrollsOverflowY() && pixelSnappedScrollHeight() != pixelSnappedClientHeight(); }
    virtual bool scrollsOverflowX() const { return hasOverflowClip() && (style()->overflowX() == OverflowScroll || hasAutoHorizontalScrollbar()); }
    virtual bool scrollsOverflowY() const { return hasOverflowClip() && (style()->overflowY() == OverflowScroll || hasAutoVerticalScrollbar()); }

    // Elements such as the <input> field override this to specify that they are scrollable
    // outside the context of the CSS overflow style
    virtual bool isIntrinsicallyScrollable(ScrollbarOrientation orientation) const { return false; }

    bool hasUnsplittableScrollingOverflow() const;

    // Page / column breakability inside block-level objects.
    enum PaginationBreakability {
        AllowAnyBreaks, // No restrictions on breaking. May examine children to find possible break points.
        ForbidBreaks, // Forbid breaks inside this object. Content cannot be split nicely into smaller pieces.
        AvoidBreaks // Preferably avoid breaks. If not possible, examine children to find possible break points.
    };
    PaginationBreakability getPaginationBreakability() const;

    LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = nullptr) override;

    virtual LayoutRect overflowClipRect(const LayoutPoint& location, OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const;
    LayoutRect clipRect(const LayoutPoint& location) const;
    virtual bool hasControlClip() const { return false; }
    virtual LayoutRect controlClipRect(const LayoutPoint&) const { return LayoutRect(); }

    virtual void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&) const;
    virtual void paintMask(const PaintInfo&, const LayoutPoint&) const;
    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;
    ResourcePriority computeResourcePriority() const final;

    void logicalExtentAfterUpdatingLogicalWidth(const LayoutUnit& logicalTop, LogicalExtentComputedValues&);

    PositionWithAffinity positionForPoint(const LayoutPoint&) override;

    void removeFloatingOrPositionedChildFromBlockLists();

    PaintLayer* enclosingFloatPaintingLayer() const;

    virtual int firstLineBoxBaseline() const { return -1; }
    virtual int inlineBlockBaseline(LineDirectionMode) const { return -1; } // Returns -1 if we should skip this box when computing the baseline of an inline-block.

    virtual Node* nodeForHitTest() const { return node(); }

    bool shrinkToAvoidFloats() const;
    virtual bool avoidsFloats() const;

    virtual void markForPaginationRelayoutIfNeeded(SubtreeLayoutScope&);

    bool isWritingModeRoot() const { return !parent() || parent()->style()->getWritingMode() != style()->getWritingMode(); }
    bool isOrthogonalWritingModeRoot() const { return parent() && parent()->isHorizontalWritingMode() != isHorizontalWritingMode(); }
    void markOrthogonalWritingModeRoot();
    void unmarkOrthogonalWritingModeRoot();

    bool isDeprecatedFlexItem() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isDeprecatedFlexibleBox(); }
    bool isFlexItemIncludingDeprecated() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isFlexibleBoxIncludingDeprecated(); }
    bool isFlexItem() const { return !isInline() && !isFloatingOrOutOfFlowPositioned() && parent() && parent()->isFlexibleBox(); }

    bool isGridItem() const { return parent() && parent()->isLayoutGrid(); }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    LayoutUnit offsetLeft(const Element*) const override;
    LayoutUnit offsetTop(const Element*) const override;

    LayoutPoint flipForWritingModeForChild(const LayoutBox* child, const LayoutPoint&) const;
    LayoutUnit flipForWritingMode(LayoutUnit position) const WARN_UNUSED_RETURN {
        // The offset is in the block direction (y for horizontal writing modes, x for vertical writing modes).
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return position;
        return logicalHeight() - position;
    }
    LayoutPoint flipForWritingMode(const LayoutPoint& position) const WARN_UNUSED_RETURN {
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return position;
        return isHorizontalWritingMode() ? LayoutPoint(position.x(), m_frameRect.height() - position.y()) : LayoutPoint(m_frameRect.width() - position.x(), position.y());
    }
    LayoutSize flipForWritingMode(const LayoutSize& offset) const WARN_UNUSED_RETURN {
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return offset;
        return LayoutSize(m_frameRect.width() - offset.width(), offset.height());
    }
    void flipForWritingMode(LayoutRect& rect) const
    {
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return;
        rect.setX(m_frameRect.width() - rect.maxX());
    }
    FloatPoint flipForWritingMode(const FloatPoint& position) const WARN_UNUSED_RETURN {
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return position;
        return FloatPoint(m_frameRect.width() - position.x(), position.y());
    }
    void flipForWritingMode(FloatRect& rect) const
    {
        if (!UNLIKELY(hasFlippedBlocksWritingMode()))
            return;
        rect.setX(m_frameRect.width() - rect.maxX());
    }
    // These represent your location relative to your container as a physical offset.
    // In layout related methods you almost always want the logical location (e.g. x() and y()).
    LayoutPoint topLeftLocation() const;
    LayoutSize topLeftLocationOffset() const { return toLayoutSize(topLeftLocation()); }

    LayoutRect logicalVisualOverflowRectForPropagation(const ComputedStyle&) const;
    LayoutRect visualOverflowRectForPropagation(const ComputedStyle&) const;
    LayoutRect logicalLayoutOverflowRectForPropagation(const ComputedStyle&) const;
    LayoutRect layoutOverflowRectForPropagation(const ComputedStyle&) const;

    bool hasOverflowModel() const { return m_overflow.get(); }
    bool hasSelfVisualOverflow() const { return m_overflow && !borderBoxRect().contains(m_overflow->selfVisualOverflowRect()); }
    bool hasVisualOverflow() const { return m_overflow && !borderBoxRect().contains(visualOverflowRect()); }

    virtual bool needsPreferredWidthsRecalculation() const;

    // See README.md for an explanation of scroll origin.
    virtual IntSize originAdjustmentForScrollbars() const;
    IntSize scrolledContentOffset() const;

    // Maps a rect in scrolling contents space to box space and apply overflow clip if needed.
    // Returns true if no clipping applied or the rect actually intersects the clipping region.
    // If edgeInclusive is true, then this method may return true even
    // if the resulting rect has zero area.
    bool mapScrollingContentsRectToBoxSpace(LayoutRect&, ApplyOverflowClipFlag, VisualRectFlags = DefaultVisualRectFlags) const;

    virtual bool hasRelativeLogicalWidth() const;
    virtual bool hasRelativeLogicalHeight() const;

    bool hasHorizontalLayoutOverflow() const
    {
        if (!m_overflow)
            return false;

        LayoutRect layoutOverflowRect = m_overflow->layoutOverflowRect();
        LayoutRect noOverflowRect = this->noOverflowRect();
        return layoutOverflowRect.x() < noOverflowRect.x() || layoutOverflowRect.maxX() > noOverflowRect.maxX();
    }

    bool hasVerticalLayoutOverflow() const
    {
        if (!m_overflow)
            return false;

        LayoutRect layoutOverflowRect = m_overflow->layoutOverflowRect();
        LayoutRect noOverflowRect = this->noOverflowRect();
        return layoutOverflowRect.y() < noOverflowRect.y() || layoutOverflowRect.maxY() > noOverflowRect.maxY();
    }

    virtual LayoutBox* createAnonymousBoxWithSameTypeAs(const LayoutObject*) const
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    bool hasSameDirectionAs(const LayoutBox* object) const { return style()->direction() == object->style()->direction(); }

    ShapeOutsideInfo* shapeOutsideInfo() const;

    void markShapeOutsideDependentsForLayout()
    {
        if (isFloating())
            removeFloatingOrPositionedChildFromBlockLists();
    }

    void setIntrinsicContentLogicalHeight(LayoutUnit intrinsicContentLogicalHeight) const { m_intrinsicContentLogicalHeight = intrinsicContentLogicalHeight; }

    bool canRenderBorderImage() const;

    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip) const override;
    void mapAncestorToLocal(const LayoutBoxModelObject*, TransformState&, MapCoordinatesFlags) const override;

    void clearPreviousPaintInvalidationRects() override;

    LayoutBlock* percentHeightContainer() const { return m_rareData ? m_rareData->m_percentHeightContainer : nullptr; }
    void setPercentHeightContainer(LayoutBlock*);
    void removeFromPercentHeightContainer();
    void clearPercentHeightDescendants();
    // For snap areas, returns the snap container that owns us.
    LayoutBox* snapContainer() const;
    void setSnapContainer(LayoutBox*);
    // For snap containers, returns all associated snap areas.
    SnapAreaSet* snapAreas() const;
    void clearSnapAreas();

    bool hitTestClippedOutByRoundedBorder(const HitTestLocation& locationInContainer, const LayoutPoint& borderBoxLocation) const;

    bool mustInvalidateFillLayersPaintOnWidthChange(const FillLayer&) const;
    bool mustInvalidateFillLayersPaintOnHeightChange(const FillLayer&) const;

protected:
    void willBeDestroyed() override;

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void updateFromStyle() override;

    // Returns false if it could not cheaply compute the extent (e.g. fixed background), in which case the returned rect may be incorrect.
    // FIXME: make this a const method once the LayoutBox reference in BoxPainter is const.
    bool getBackgroundPaintedExtent(LayoutRect&) const;
    virtual bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const;
    bool computeBackgroundIsKnownToBeObscured() const override;

    virtual void computePositionedLogicalWidth(LogicalExtentComputedValues&) const;

    LayoutUnit computeIntrinsicLogicalWidthUsing(const Length& logicalWidthLength, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const;
    virtual LayoutUnit computeIntrinsicLogicalContentHeightUsing(const Length& logicalHeightLength, LayoutUnit intrinsicContentHeight, LayoutUnit borderAndPadding) const;

    virtual bool shouldComputeSizeAsReplaced() const { return isAtomicInlineLevel() && !isInlineBlockOrInlineTable(); }

    LayoutObject* splitAnonymousBoxesAroundChild(LayoutObject* beforeChild);

    virtual bool hitTestOverflowControl(HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }
    virtual bool hitTestChildren(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    void addLayerHitTestRects(LayerHitTestRects&, const PaintLayer* currentCompositedLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const override;
    void computeSelfHitTestRects(Vector<LayoutRect>&, const LayoutPoint& layerOffset) const override;

    PaintInvalidationReason getPaintInvalidationReason(const PaintInvalidationState&,
        const LayoutRect& oldBounds, const LayoutPoint& oldPositionFromPaintInvalidationContainer,
        const LayoutRect& newBounds, const LayoutPoint& newPositionFromPaintInvalidationContainer) const override;
    void incrementallyInvalidatePaint(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect& oldBounds, const LayoutRect& newBounds, const LayoutPoint& positionFromPaintInvalidationContainer) override;

    PaintInvalidationReason invalidatePaintIfNeeded(const PaintInvalidationState&) override;
    void invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState& childPaintInvalidationState) override;

    bool hasStretchedLogicalWidth() const;

    bool hasNonCompositedScrollbars() const final;
    void excludeScrollbars(LayoutRect&, OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const;

    LayoutUnit containingBlockLogicalWidthForPositioned(const LayoutBoxModelObject* containingBlock, bool checkForPerpendicularWritingMode = true) const;
    LayoutUnit containingBlockLogicalHeightForPositioned(const LayoutBoxModelObject* containingBlock, bool checkForPerpendicularWritingMode = true) const;

    static void computeBlockStaticDistance(Length& logicalTop, Length& logicalBottom, const LayoutBox* child, const LayoutBoxModelObject* containerBlock);
    static void computeInlineStaticDistance(Length& logicalLeft, Length& logicalRight, const LayoutBox* child, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth);
    static void computeLogicalLeftPositionedOffset(LayoutUnit& logicalLeftPos, const LayoutBox* child, LayoutUnit logicalWidthValue, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth);
    static void computeLogicalTopPositionedOffset(LayoutUnit& logicalTopPos, const LayoutBox* child, LayoutUnit logicalHeightValue, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalHeight);

private:
    bool mustInvalidateBackgroundOrBorderPaintOnHeightChange() const;
    bool mustInvalidateBackgroundOrBorderPaintOnWidthChange() const;

    void invalidatePaintRectClippedByOldAndNewBounds(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect&, const LayoutRect& oldBounds, const LayoutRect& newBounds);

    void updateShapeOutsideInfoAfterStyleChange(const ComputedStyle&, const ComputedStyle* oldStyle);
    void updateGridPositionAfterStyleChange(const ComputedStyle*);
    void updateScrollSnapMappingAfterStyleChange(const ComputedStyle*, const ComputedStyle* oldStyle);
    void clearScrollSnapMapping();
    void addScrollSnapMapping();

    bool autoWidthShouldFitContent() const;
    LayoutUnit shrinkToFitLogicalWidth(LayoutUnit availableLogicalWidth, LayoutUnit bordersPlusPadding) const;

    // Returns true if we queued up a paint invalidation.
    bool invalidatePaintOfLayerRectsForImage(WrappedImagePtr, const FillLayer&, bool drawingBackground);

    bool stretchesToViewportInQuirksMode() const;
    bool skipContainingBlockForPercentHeightCalculation(const LayoutBox* containingBlock) const;

    virtual void computePositionedLogicalHeight(LogicalExtentComputedValues&) const;
    void computePositionedLogicalWidthUsing(SizeType, Length logicalWidth, const LayoutBoxModelObject* containerBlock, TextDirection containerDirection,
        LayoutUnit containerLogicalWidth, LayoutUnit bordersPlusPadding,
        const Length& logicalLeft, const Length& logicalRight, const Length& marginLogicalLeft,
        const Length& marginLogicalRight, LogicalExtentComputedValues&) const;
    void computePositionedLogicalHeightUsing(SizeType, Length logicalHeightLength, const LayoutBoxModelObject* containerBlock,
        LayoutUnit containerLogicalHeight, LayoutUnit bordersPlusPadding, LayoutUnit logicalHeight,
        const Length& logicalTop, const Length& logicalBottom, const Length& marginLogicalTop,
        const Length& marginLogicalBottom, LogicalExtentComputedValues&) const;

    LayoutUnit fillAvailableMeasure(LayoutUnit availableLogicalWidth) const;
    LayoutUnit fillAvailableMeasure(LayoutUnit availableLogicalWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd) const;

    // Calculates the intrinsic(https://drafts.csswg.org/css-sizing-3/#intrinsic) logical widths for this layout box.
    //
    // intrinsicWidth is defined as:
    //     intrinsic size of content (without our border and padding) + scrollbarWidth.
    //
    // preferredWidth is defined as:
    //     fixedWidth OR (intrinsicWidth plus border and padding).
    //     Note: fixedWidth includes border and padding and scrollbarWidth.
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    // This function calculates the preferred widths for an object.
    //
    // This function is only expected to be called if
    // the boolean preferredLogicalWidthsDirty is true. It also MUST clear the
    // boolean before returning.
    //
    // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS in LayoutObject.h for more
    // details about those widths.
    virtual void computePreferredLogicalWidths() { clearPreferredLogicalWidthsDirty(); }

    LayoutBoxRareData& ensureRareData()
    {
        if (!m_rareData)
            m_rareData = wrapUnique(new LayoutBoxRareData());
        return *m_rareData.get();
    }

    bool needToSavePreviousBoxSizes();
    void savePreviousBoxSizesIfNeeded();
    LayoutSize computePreviousBorderBoxSize(const LayoutSize& previousBoundsSize) const;

    bool logicalHeightComputesAsNone(SizeType) const;

    bool isBox() const = delete; // This will catch anyone doing an unnecessary check.

    void frameRectChanged()
    {
        // The frame rect may change because of layout of other objects.
        // Should check this object for paint invalidation.
        if (!needsLayout())
            setMayNeedPaintInvalidation();
    }

    // Returns true if the box intersects the viewport visible to the user.
    bool intersectsVisibleViewport();

    virtual bool isInSelfHitTestingPhase(HitTestAction hitTestAction) const { return hitTestAction == HitTestForeground; }

    void updateBackgroundAttachmentFixedStatusAfterStyleChange();

    // The CSS border box rect for this box.
    //
    // The rectangle is in this box's physical coordinates but with a
    // flipped block-flow direction (see the COORDINATE SYSTEMS section
    // in LayoutBoxModelObject). The location is the distance from this
    // object's border edge to the container's border edge (which is not
    // always the parent). Thus it includes any logical top/left along
    // with this box's margins.
    LayoutRect m_frameRect;

    // Our intrinsic height, used for min-height: min-content etc. Maintained by
    // updateLogicalHeight. This is logicalHeight() before it is clamped to
    // min/max.
    mutable LayoutUnit m_intrinsicContentLogicalHeight;

    void inflateVisualRectForReflectionAndFilter(LayoutRect&) const;
    void inflateVisualRectForReflectionAndFilterUnderContainer(LayoutRect&, const LayoutObject& container, const LayoutBoxModelObject* ancestorToStopAt) const;

    LayoutRectOutsets m_marginBoxOutsets;

    void addSnapArea(const LayoutBox&);
    void removeSnapArea(const LayoutBox&);

protected:
    // The logical width of the element if it were to break its lines at every
    // possible opportunity.
    //
    // See LayoutObject::minPreferredLogicalWidth() for more details.
    LayoutUnit m_minPreferredLogicalWidth;

    // The logical width of the element if it never breaks any lines at all.
    //
    // See LayoutObject::maxPreferredLogicalWidth() for more details.
    LayoutUnit m_maxPreferredLogicalWidth;

    // Our overflow information.
    std::unique_ptr<BoxOverflowModel> m_overflow;

private:
    // The inline box containing this LayoutBox, for atomic inline elements.
    InlineBox* m_inlineBoxWrapper;

    std::unique_ptr<LayoutBoxRareData> m_rareData;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutBox, isBox());

inline LayoutBox* LayoutBox::previousSiblingBox() const
{
    return toLayoutBox(previousSibling());
}

inline LayoutBox* LayoutBox::previousInFlowSiblingBox() const
{
    LayoutBox* previous = previousSiblingBox();
    while (previous && previous->isOutOfFlowPositioned())
        previous = previous->previousSiblingBox();
    return previous;
}

inline LayoutBox* LayoutBox::nextSiblingBox() const
{
    return toLayoutBox(nextSibling());
}

inline LayoutBox* LayoutBox::nextInFlowSiblingBox() const
{
    LayoutBox* next = nextSiblingBox();
    while (next && next->isOutOfFlowPositioned())
        next = next->nextSiblingBox();
    return next;
}

inline LayoutBox* LayoutBox::parentBox() const
{
    return toLayoutBox(parent());
}

inline LayoutBox* LayoutBox::firstChildBox() const
{
    return toLayoutBox(slowFirstChild());
}

inline LayoutBox* LayoutBox::firstInFlowChildBox() const
{
    LayoutBox* child = firstChildBox();
    while (child && child->isOutOfFlowPositioned())
        child = child->nextSiblingBox();
    return child;
}

inline LayoutBox* LayoutBox::lastChildBox() const
{
    return toLayoutBox(slowLastChild());
}

inline LayoutBox* LayoutBox::previousSiblingMultiColumnBox() const
{
    ASSERT(isLayoutMultiColumnSpannerPlaceholder() || isLayoutMultiColumnSet());
    LayoutBox* previousBox = previousSiblingBox();
    if (previousBox->isLayoutFlowThread())
        return nullptr;
    return previousBox;
}

inline LayoutBox* LayoutBox::nextSiblingMultiColumnBox() const
{
    ASSERT(isLayoutMultiColumnSpannerPlaceholder() || isLayoutMultiColumnSet());
    return nextSiblingBox();
}

inline void LayoutBox::setInlineBoxWrapper(InlineBox* boxWrapper)
{
    if (boxWrapper) {
        ASSERT(!m_inlineBoxWrapper);
        // m_inlineBoxWrapper should already be nullptr. Deleting it is a safeguard against security issues.
        // Otherwise, there will two line box wrappers keeping the reference to this layoutObject, and
        // only one will be notified when the layoutObject is getting destroyed. The second line box wrapper
        // will keep a stale reference.
        if (UNLIKELY(m_inlineBoxWrapper != nullptr))
            deleteLineBoxWrapper();
    }

    m_inlineBoxWrapper = boxWrapper;
}

inline bool LayoutBox::isForcedFragmentainerBreakValue(EBreak breakValue)
{
    return breakValue == BreakColumn
        || breakValue == BreakLeft
        || breakValue == BreakPage
        || breakValue == BreakRecto
        || breakValue == BreakRight
        || breakValue == BreakVerso;
}

} // namespace blink

#endif // LayoutBox_h
