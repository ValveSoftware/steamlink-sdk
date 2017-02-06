/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "core/layout/LayoutBox.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutDeprecatedFlexibleBox.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutReplica.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LineLayoutBox.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/layout/shapes/ShapeOutsideInfo.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/SnapCoordinator.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <math.h>

namespace blink {

// Used by flexible boxes when flexing this element and by table cells.
typedef WTF::HashMap<const LayoutBox*, LayoutUnit> OverrideSizeMap;

// Used by grid elements to properly size their grid items.
// FIXME: Move these into LayoutBoxRareData.
static OverrideSizeMap* gOverrideContainingBlockLogicalHeightMap = nullptr;
static OverrideSizeMap* gOverrideContainingBlockLogicalWidthMap = nullptr;
static OverrideSizeMap* gExtraInlineOffsetMap = nullptr;
static OverrideSizeMap* gExtraBlockOffsetMap = nullptr;


// Size of border belt for autoscroll. When mouse pointer in border belt,
// autoscroll is started.
static const int autoscrollBeltSize = 20;
static const unsigned backgroundObscurationTestMaxDepth = 4;

struct SameSizeAsLayoutBox : public LayoutBoxModelObject {
    LayoutRect frameRect;
    LayoutUnit intrinsicContentLogicalHeight;
    LayoutRectOutsets marginBoxOutsets;
    LayoutUnit preferredLogicalWidth[2];
    void* pointers[3];
};

static_assert(sizeof(LayoutBox) == sizeof(SameSizeAsLayoutBox), "LayoutBox should stay small");

LayoutBox::LayoutBox(ContainerNode* node)
    : LayoutBoxModelObject(node)
    , m_intrinsicContentLogicalHeight(-1)
    , m_minPreferredLogicalWidth(-1)
    , m_maxPreferredLogicalWidth(-1)
    , m_inlineBoxWrapper(nullptr)
{
    setIsBox();
}

PaintLayerType LayoutBox::layerTypeRequired() const
{
    // hasAutoZIndex only returns true if the element is positioned or a flex-item since
    // position:static elements that are not flex-items get their z-index coerced to auto.
    if (isPositioned() || createsGroup() || hasClipPath() || hasTransformRelatedProperty()
        || style()->hasCompositorProxy() || hasHiddenBackface() || hasReflection() || style()->specifiesColumns()
        || !style()->hasAutoZIndex() || style()->shouldCompositeForCurrentAnimations())
        return NormalPaintLayer;

    if (hasOverflowClip())
        return OverflowClipPaintLayer;

    return NoPaintLayer;
}

void LayoutBox::willBeDestroyed()
{
    clearOverrideSize();
    clearContainingBlockOverrideSize();
    clearExtraInlineAndBlockOffests();

    if (isOutOfFlowPositioned())
        LayoutBlock::removePositionedObject(this);
    removeFromPercentHeightContainer();
    if (isOrthogonalWritingModeRoot() && !documentBeingDestroyed())
        unmarkOrthogonalWritingModeRoot();

    ShapeOutsideInfo::removeInfo(*this);

    LayoutBoxModelObject::willBeDestroyed();
}

void LayoutBox::insertedIntoTree()
{
    LayoutBoxModelObject::insertedIntoTree();
    addScrollSnapMapping();

    if (isOrthogonalWritingModeRoot())
        markOrthogonalWritingModeRoot();
}

void LayoutBox::willBeRemovedFromTree()
{
    if (!documentBeingDestroyed() && isOrthogonalWritingModeRoot())
        unmarkOrthogonalWritingModeRoot();

    clearScrollSnapMapping();
    LayoutBoxModelObject::willBeRemovedFromTree();
}

void LayoutBox::removeFloatingOrPositionedChildFromBlockLists()
{
    ASSERT(isFloatingOrOutOfFlowPositioned());

    if (documentBeingDestroyed())
        return;

    if (isFloating()) {
        LayoutBlockFlow* parentBlockFlow = nullptr;
        for (LayoutObject* curr = parent(); curr; curr = curr->parent()) {
            if (curr->isLayoutBlockFlow()) {
                LayoutBlockFlow* currBlockFlow = toLayoutBlockFlow(curr);
                if (!parentBlockFlow || currBlockFlow->containsFloat(this))
                    parentBlockFlow = currBlockFlow;
            }
        }

        if (parentBlockFlow) {
            parentBlockFlow->markSiblingsWithFloatsForLayout(this);
            parentBlockFlow->markAllDescendantsWithFloatsForLayout(this, false);
        }
    }

    if (isOutOfFlowPositioned())
        LayoutBlock::removePositionedObject(this);
}

void LayoutBox::styleWillChange(StyleDifference diff, const ComputedStyle& newStyle)
{
    const ComputedStyle* oldStyle = style();
    if (oldStyle) {
        LayoutFlowThread* flowThread = flowThreadContainingBlock();
        if (flowThread && flowThread != this)
            flowThread->flowThreadDescendantStyleWillChange(this, diff, newStyle);

        // The background of the root element or the body element could propagate up to
        // the canvas. Just dirty the entire canvas when our style changes substantially.
        if ((diff.needsPaintInvalidation() || diff.needsLayout()) && node()
            && (isHTMLHtmlElement(*node()) || isHTMLBodyElement(*node()))) {
            view()->setShouldDoFullPaintInvalidation();

            if (oldStyle->hasEntirelyFixedBackground() != newStyle.hasEntirelyFixedBackground())
                view()->compositor()->setNeedsUpdateFixedBackground();
        }

        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the layout tree using the old position value now.
        if (diff.needsFullLayout() && parent() && oldStyle->position() != newStyle.position()) {
            if (!oldStyle->hasOutOfFlowPosition() && newStyle.hasOutOfFlowPosition()) {
                // We're about to go out of flow. Before that takes place, we need to mark the
                // current containing block chain for preferred widths recalculation.
                setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::StyleChange);
            } else {
                markContainerChainForLayout();
            }
            if (oldStyle->position() == StaticPosition)
                setShouldDoFullPaintInvalidation();
            else if (newStyle.hasOutOfFlowPosition())
                parent()->setChildNeedsLayout();
            if (isFloating() && !isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition())
                removeFloatingOrPositionedChildFromBlockLists();
        }
    // FIXME: This branch runs when !oldStyle, which means that layout was never called
    // so what's the point in invalidating the whole view that we never painted?
    } else if (isBody()) {
        view()->setShouldDoFullPaintInvalidation();
    }

    LayoutBoxModelObject::styleWillChange(diff, newStyle);
}

void LayoutBox::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    // Horizontal writing mode definition is updated in LayoutBoxModelObject::updateFromStyle,
    // (as part of the LayoutBoxModelObject::styleDidChange call below). So, we can safely cache the horizontal
    // writing mode value before style change here.
    bool oldHorizontalWritingMode = isHorizontalWritingMode();

    LayoutBoxModelObject::styleDidChange(diff, oldStyle);

    if (isFloatingOrOutOfFlowPositioned() && oldStyle && !oldStyle->isFloating() && !oldStyle->hasOutOfFlowPosition() && parent() && parent()->isLayoutBlockFlow())
        toLayoutBlockFlow(parent())->childBecameFloatingOrOutOfFlow(this);

    const ComputedStyle& newStyle = styleRef();
    if (needsLayout() && oldStyle)
        removeFromPercentHeightContainer();

    if (oldHorizontalWritingMode != isHorizontalWritingMode()) {
        if (oldStyle) {
            if (isOrthogonalWritingModeRoot())
                markOrthogonalWritingModeRoot();
            else
                unmarkOrthogonalWritingModeRoot();
        }

        clearPercentHeightDescendants();
    }

    // If our zoom factor changes and we have a defined scrollLeft/Top, we need to adjust that value into the
    // new zoomed coordinate space.
    if (hasOverflowClip() && oldStyle && oldStyle->effectiveZoom() != newStyle.effectiveZoom()) {
        PaintLayerScrollableArea* scrollableArea = this->getScrollableArea();
        ASSERT(scrollableArea);
        if (int left = scrollableArea->scrollXOffset()) {
            left = (left / oldStyle->effectiveZoom()) * newStyle.effectiveZoom();
            scrollableArea->scrollToXOffset(left);
        }
        if (int top = scrollableArea->scrollYOffset()) {
            top = (top / oldStyle->effectiveZoom()) * newStyle.effectiveZoom();
            scrollableArea->scrollToYOffset(top);
        }
    }

    // Our opaqueness might have changed without triggering layout.
    if (diff.needsPaintInvalidation()) {
        LayoutObject* parentToInvalidate = parent();
        for (unsigned i = 0; i < backgroundObscurationTestMaxDepth && parentToInvalidate; ++i) {
            parentToInvalidate->invalidateBackgroundObscurationStatus();
            parentToInvalidate = parentToInvalidate->parent();
        }
    }

    if (isDocumentElement() || isBody()) {
        document().view()->recalculateScrollbarOverlayStyle(document().view()->documentBackgroundColor());
        document().view()->recalculateCustomScrollbarStyle();
        if (LayoutView* layoutView = view()) {
            if (PaintLayerScrollableArea* scrollableArea = layoutView->getScrollableArea()) {
                if (scrollableArea->horizontalScrollbar() && scrollableArea->horizontalScrollbar()->isCustomScrollbar())
                    scrollableArea->horizontalScrollbar()->styleChanged();
                if (scrollableArea->verticalScrollbar() && scrollableArea->verticalScrollbar()->isCustomScrollbar())
                    scrollableArea->verticalScrollbar()->styleChanged();
            }
        }
    }
    updateShapeOutsideInfoAfterStyleChange(*style(), oldStyle);
    updateGridPositionAfterStyleChange(oldStyle);

    if (LayoutMultiColumnSpannerPlaceholder* placeholder = this->spannerPlaceholder())
        placeholder->layoutObjectInFlowThreadStyleDidChange(oldStyle);

    updateBackgroundAttachmentFixedStatusAfterStyleChange();

    if (oldStyle) {
        LayoutFlowThread* flowThread = flowThreadContainingBlock();
        if (flowThread && flowThread != this)
            flowThread->flowThreadDescendantStyleDidChange(this, diff, *oldStyle);

        updateScrollSnapMappingAfterStyleChange(&newStyle, oldStyle);
    }

    ASSERT(!isInline() || isAtomicInlineLevel()); // Non-atomic inlines should be LayoutInline or LayoutText, not LayoutBox.
}

void LayoutBox::updateBackgroundAttachmentFixedStatusAfterStyleChange()
{
    if (!frameView())
        return;

    // On low-powered/mobile devices, preventing blitting on a scroll can cause noticeable delays
    // when scrolling a page with a fixed background image. As an optimization, assuming there are
    // no fixed positoned elements on the page, we can acclerate scrolling (via blitting) if we
    // ignore the CSS property "background-attachment: fixed".
    bool ignoreFixedBackgroundAttachment = RuntimeEnabledFeatures::fastMobileScrollingEnabled();
    if (ignoreFixedBackgroundAttachment)
        return;

    // An object needs to be repainted on frame scroll when it has background-attachment:fixed.
    // LayoutView is responsible for painting root background, thus the root element (and the
    // body element if html element has no background) skips painting backgrounds.
    bool isBackgroundAttachmentFixedObject = !isDocumentElement() && !backgroundStolenForBeingBody() && styleRef().hasFixedBackgroundImage();
    if (isLayoutView() && view()->compositor()->supportsFixedRootBackgroundCompositing()) {
        if (styleRef().hasEntirelyFixedBackground())
            isBackgroundAttachmentFixedObject = false;
    }

    setIsBackgroundAttachmentFixedObject(isBackgroundAttachmentFixedObject);
}

void LayoutBox::updateShapeOutsideInfoAfterStyleChange(const ComputedStyle& style, const ComputedStyle* oldStyle)
{
    const ShapeValue* shapeOutside = style.shapeOutside();
    const ShapeValue* oldShapeOutside = oldStyle ? oldStyle->shapeOutside() : ComputedStyle::initialShapeOutside();

    Length shapeMargin = style.shapeMargin();
    Length oldShapeMargin = oldStyle ? oldStyle->shapeMargin() : ComputedStyle::initialShapeMargin();

    float shapeImageThreshold = style.shapeImageThreshold();
    float oldShapeImageThreshold = oldStyle ? oldStyle->shapeImageThreshold() : ComputedStyle::initialShapeImageThreshold();

    // FIXME: A future optimization would do a deep comparison for equality. (bug 100811)
    if (shapeOutside == oldShapeOutside && shapeMargin == oldShapeMargin && shapeImageThreshold == oldShapeImageThreshold)
        return;

    if (!shapeOutside)
        ShapeOutsideInfo::removeInfo(*this);
    else
        ShapeOutsideInfo::ensureInfo(*this).markShapeAsDirty();

    if (shapeOutside || shapeOutside != oldShapeOutside)
        markShapeOutsideDependentsForLayout();
}

void LayoutBox::updateGridPositionAfterStyleChange(const ComputedStyle* oldStyle)
{
    if (!oldStyle || !parent() || !parent()->isLayoutGrid())
        return;

    if (oldStyle->gridColumnStart() == style()->gridColumnStart()
        && oldStyle->gridColumnEnd() == style()->gridColumnEnd()
        && oldStyle->gridRowStart() == style()->gridRowStart()
        && oldStyle->gridRowEnd() == style()->gridRowEnd()
        && oldStyle->order() == style()->order()
        && oldStyle->hasOutOfFlowPosition() == style()->hasOutOfFlowPosition())
        return;

    // It should be possible to not dirty the grid in some cases (like moving an explicitly placed grid item).
    // For now, it's more simple to just always recompute the grid.
    toLayoutGrid(parent())->dirtyGrid();
}

void LayoutBox::updateScrollSnapMappingAfterStyleChange(const ComputedStyle* newStyle, const ComputedStyle* oldStyle)
{
    SnapCoordinator* snapCoordinator = document().snapCoordinator();
    if (!snapCoordinator)
        return;

    // Scroll snap type has no effect on the viewport defining element instead
    // they are handled by the LayoutView.
    bool allowsSnapContainer = node() != document().viewportDefiningElement();

    ScrollSnapType oldSnapType = oldStyle ? oldStyle->getScrollSnapType() : ScrollSnapTypeNone;
    ScrollSnapType newSnapType = newStyle && allowsSnapContainer ? newStyle->getScrollSnapType() : ScrollSnapTypeNone;
    if (oldSnapType != newSnapType)
        snapCoordinator->snapContainerDidChange(*this, newSnapType);

    Vector<LengthPoint> emptyVector;
    const Vector<LengthPoint>& oldSnapCoordinate = oldStyle ? oldStyle->scrollSnapCoordinate() : emptyVector;
    const Vector<LengthPoint>& newSnapCoordinate = newStyle ? newStyle->scrollSnapCoordinate() : emptyVector;
    if (oldSnapCoordinate != newSnapCoordinate)
        snapCoordinator->snapAreaDidChange(*this, newSnapCoordinate);
}

void LayoutBox::addScrollSnapMapping()
{
    updateScrollSnapMappingAfterStyleChange(style(), nullptr);
}

void LayoutBox::clearScrollSnapMapping()
{
    updateScrollSnapMappingAfterStyleChange(nullptr, style());
}

void LayoutBox::updateFromStyle()
{
    LayoutBoxModelObject::updateFromStyle();

    const ComputedStyle& styleToUse = styleRef();
    setFloating(!isOutOfFlowPositioned() && styleToUse.isFloating());
    setHasTransformRelatedProperty(styleToUse.hasTransformRelatedProperty());
    setHasReflection(styleToUse.boxReflect());
}

void LayoutBox::layout()
{
    ASSERT(needsLayout());
    LayoutAnalyzer::Scope analyzer(*this);

    LayoutObject* child = slowFirstChild();
    if (!child) {
        clearNeedsLayout();
        return;
    }

    LayoutState state(*this, locationOffset());
    while (child) {
        child->layoutIfNeeded();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }
    invalidateBackgroundObscurationStatus();
    clearNeedsLayout();
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
LayoutUnit LayoutBox::clientWidth() const
{
    return m_frameRect.width() - borderLeft() - borderRight() - verticalScrollbarWidth();
}

LayoutUnit LayoutBox::clientHeight() const
{
    return m_frameRect.height() - borderTop() - borderBottom() - horizontalScrollbarHeight();
}

int LayoutBox::pixelSnappedClientWidth() const
{
    return snapSizeToPixel(clientWidth(), location().x() + clientLeft());
}

int LayoutBox::pixelSnappedClientHeight() const
{
    return snapSizeToPixel(clientHeight(), location().y() + clientTop());
}

int LayoutBox::pixelSnappedOffsetWidth(const Element*) const
{
    return snapSizeToPixel(offsetWidth(), location().x() + clientLeft());
}

int LayoutBox::pixelSnappedOffsetHeight(const Element*) const
{
    return snapSizeToPixel(offsetHeight(), location().y() + clientTop());
}

LayoutUnit LayoutBox::scrollWidth() const
{
    if (hasOverflowClip())
        return getScrollableArea()->scrollWidth();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    if (style()->isLeftToRightDirection())
        return std::max(clientWidth(), layoutOverflowRect().maxX() - borderLeft());
    return clientWidth() - std::min(LayoutUnit(), layoutOverflowRect().x() - borderLeft());
}

LayoutUnit LayoutBox::scrollHeight() const
{
    if (hasOverflowClip())
        return getScrollableArea()->scrollHeight();
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    return std::max(clientHeight(), layoutOverflowRect().maxY() - borderTop());
}

LayoutUnit LayoutBox::scrollLeft() const
{
    return hasOverflowClip() ? LayoutUnit(getScrollableArea()->scrollXOffset()) : LayoutUnit();
}

LayoutUnit LayoutBox::scrollTop() const
{
    return hasOverflowClip() ? LayoutUnit(getScrollableArea()->scrollYOffset()) : LayoutUnit();
}

int LayoutBox::pixelSnappedScrollWidth() const
{
    return snapSizeToPixel(scrollWidth(), location().x() + clientLeft());
}

int LayoutBox::pixelSnappedScrollHeight() const
{
    if (hasOverflowClip())
        return snapSizeToPixel(getScrollableArea()->scrollHeight(), location().y() + clientTop());
    // For objects with visible overflow, this matches IE.
    // FIXME: Need to work right with writing modes.
    return snapSizeToPixel(scrollHeight(), location().y() + clientTop());
}

void LayoutBox::setScrollLeft(LayoutUnit newLeft)
{
    // This doesn't hit in any tests, but since the equivalent code in setScrollTop
    // does, presumably this code does as well.
    DisableCompositingQueryAsserts disabler;

    if (hasOverflowClip())
        getScrollableArea()->scrollToXOffset(newLeft, ScrollOffsetClamped, ScrollBehaviorAuto);
}

void LayoutBox::setScrollTop(LayoutUnit newTop)
{
    // Hits in compositing/overflow/do-not-assert-on-invisible-composited-layers.html
    DisableCompositingQueryAsserts disabler;

    if (hasOverflowClip())
        getScrollableArea()->scrollToYOffset(newTop, ScrollOffsetClamped, ScrollBehaviorAuto);
}

void LayoutBox::scrollToOffset(const DoubleSize& offset, ScrollBehavior scrollBehavior)
{
    // This doesn't hit in any tests, but since the equivalent code in setScrollTop
    // does, presumably this code does as well.
    DisableCompositingQueryAsserts disabler;

    if (hasOverflowClip())
        getScrollableArea()->scrollToOffset(offset, ScrollOffsetClamped, scrollBehavior);
}

// Returns true iff we are attempting an autoscroll inside an iframe with scrolling="no".
static bool isDisallowedAutoscroll(HTMLFrameOwnerElement* ownerElement, FrameView* frameView)
{
    if (ownerElement && isHTMLFrameElementBase(*ownerElement)) {
        HTMLFrameElementBase* frameElementBase = toHTMLFrameElementBase(ownerElement);
        if (Page* page = frameView->frame().page()) {
            return page->autoscrollController().autoscrollInProgress()
                && frameElementBase->scrollingMode() == ScrollbarAlwaysOff;
        }
    }
    return false;
}

void LayoutBox::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType scrollType, bool makeVisibleInVisualViewport)
{
    ASSERT(scrollType == ProgrammaticScroll || scrollType == UserScroll);
    // Presumably the same issue as in setScrollTop. See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    LayoutBox* parentBox = nullptr;
    LayoutRect newRect = rect;

    bool restrictedByLineClamp = false;
    if (parent()) {
        parentBox = parent()->enclosingBox();
        restrictedByLineClamp = !parent()->style()->lineClamp().isNone();
    }

    if (hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        newRect = getScrollableArea()->scrollIntoView(rect, alignX, alignY, scrollType);
    } else if (!parentBox && canBeProgramaticallyScrolled()) {
        if (FrameView* frameView = this->frameView()) {
            HTMLFrameOwnerElement* ownerElement = document().localOwner();
            if (!isDisallowedAutoscroll(ownerElement, frameView)) {
                if (makeVisibleInVisualViewport) {
                    frameView->getScrollableArea()->scrollIntoView(rect, alignX, alignY, scrollType);
                } else {
                    frameView->layoutViewportScrollableArea()->scrollIntoView(rect, alignX, alignY, scrollType);
                }
                if (ownerElement && ownerElement->layoutObject()) {
                    if (frameView->safeToPropagateScrollToParent()) {
                        parentBox = ownerElement->layoutObject()->enclosingBox();
                        LayoutView* parentView = ownerElement->layoutObject()->view();
                        newRect = enclosingLayoutRect(view()->localToAncestorQuad(FloatRect(rect), parentView, UseTransforms | TraverseDocumentBoundaries).boundingBox());
                    } else {
                        parentBox = nullptr;
                    }
                }
            }
        }
    }

    // If we are fixed-position, it is useless to scroll the parent.
    if (hasLayer() && layer()->scrollsWithViewport())
        return;

    if (frame()->page()->autoscrollController().autoscrollInProgress())
        parentBox = enclosingScrollableBox();

    if (parentBox)
        parentBox->scrollRectToVisible(newRect, alignX, alignY, scrollType, makeVisibleInVisualViewport);
}

void LayoutBox::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    rects.append(pixelSnappedIntRect(accumulatedOffset, size()));
}

void LayoutBox::absoluteQuads(Vector<FloatQuad>& quads) const
{
    quads.append(localToAbsoluteQuad(FloatRect(0, 0, m_frameRect.width().toFloat(), m_frameRect.height().toFloat())));
}

void LayoutBox::updateLayerTransformAfterLayout()
{
    // Transform-origin depends on box size, so we need to update the layer transform after layout.
    if (hasLayer())
        layer()->updateTransformationMatrix();
}

LayoutUnit LayoutBox::constrainLogicalWidthByMinMax(LayoutUnit logicalWidth, LayoutUnit availableWidth, LayoutBlock* cb) const
{
    const ComputedStyle& styleToUse = styleRef();
    if (!styleToUse.logicalMaxWidth().isMaxSizeNone())
        logicalWidth = std::min(logicalWidth, computeLogicalWidthUsing(MaxSize, styleToUse.logicalMaxWidth(), availableWidth, cb));
    return std::max(logicalWidth, computeLogicalWidthUsing(MinSize, styleToUse.logicalMinWidth(), availableWidth, cb));
}

LayoutUnit LayoutBox::constrainLogicalHeightByMinMax(LayoutUnit logicalHeight, LayoutUnit intrinsicContentHeight) const
{
    const ComputedStyle& styleToUse = styleRef();
    if (!styleToUse.logicalMaxHeight().isMaxSizeNone()) {
        LayoutUnit maxH = computeLogicalHeightUsing(MaxSize, styleToUse.logicalMaxHeight(), intrinsicContentHeight);
        if (maxH != -1)
            logicalHeight = std::min(logicalHeight, maxH);
    }
    return std::max(logicalHeight, computeLogicalHeightUsing(MinSize, styleToUse.logicalMinHeight(), intrinsicContentHeight));
}

LayoutUnit LayoutBox::constrainContentBoxLogicalHeightByMinMax(LayoutUnit logicalHeight, LayoutUnit intrinsicContentHeight) const
{
    // If the min/max height and logical height are both percentages we take advantage of already knowing the current resolved percentage height
    // to avoid recursing up through our containing blocks again to determine it.
    const ComputedStyle& styleToUse = styleRef();
    if (!styleToUse.logicalMaxHeight().isMaxSizeNone()) {
        if (styleToUse.logicalMaxHeight().type() == Percent && styleToUse.logicalHeight().type() == Percent) {
            LayoutUnit availableLogicalHeight(logicalHeight / styleToUse.logicalHeight().value() * 100);
            logicalHeight = std::min(logicalHeight, valueForLength(styleToUse.logicalMaxHeight(), availableLogicalHeight));
        } else {
            LayoutUnit maxHeight(computeContentLogicalHeight(MaxSize, styleToUse.logicalMaxHeight(), intrinsicContentHeight));
            if (maxHeight != -1)
                logicalHeight = std::min(logicalHeight, maxHeight);
        }
    }

    if (styleToUse.logicalMinHeight().type() == Percent && styleToUse.logicalHeight().type() == Percent) {
        LayoutUnit availableLogicalHeight(logicalHeight / styleToUse.logicalHeight().value() * 100);
        logicalHeight = std::max(logicalHeight, valueForLength(styleToUse.logicalMinHeight(), availableLogicalHeight));
    } else {
        logicalHeight = std::max(logicalHeight, computeContentLogicalHeight(MinSize, styleToUse.logicalMinHeight(), intrinsicContentHeight));
    }

    return logicalHeight;
}

void LayoutBox::setLocationAndUpdateOverflowControlsIfNeeded(const LayoutPoint& location)
{
    if (hasOverflowClip()) {
        IntSize oldPixelSnappedBorderRectSize = pixelSnappedBorderBoxRect().size();
        setLocation(location);
        if (pixelSnappedBorderBoxRect().size() != oldPixelSnappedBorderRectSize)
            getScrollableArea()->updateAfterLayout();
        return;
    }

    setLocation(location);
}

IntRect LayoutBox::absoluteContentBox() const
{
    // This is wrong with transforms and flipped writing modes.
    IntRect rect = pixelSnappedIntRect(contentBoxRect());
    FloatPoint absPos = localToAbsolute();
    rect.move(absPos.x(), absPos.y());
    return rect;
}

IntSize LayoutBox::absoluteContentBoxOffset() const
{
    IntPoint offset = roundedIntPoint(contentBoxOffset());
    FloatPoint absPos = localToAbsolute();
    offset.move(absPos.x(), absPos.y());
    return toIntSize(offset);
}

FloatQuad LayoutBox::absoluteContentQuad() const
{
    LayoutRect rect = contentBoxRect();
    return localToAbsoluteQuad(FloatRect(rect));
}

void LayoutBox::addOutlineRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot) const
{
    rects.append(LayoutRect(additionalOffset, size()));
}

bool LayoutBox::canResize() const
{
    // We need a special case for <iframe> because they never have
    // hasOverflowClip(). However, they do "implicitly" clip their contents, so
    // we want to allow resizing them also.
    return (hasOverflowClip() || isLayoutIFrame()) && style()->resize() != RESIZE_NONE;
}

void LayoutBox::addLayerHitTestRects(LayerHitTestRects& layerRects, const PaintLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    LayoutPoint adjustedLayerOffset = layerOffset + locationOffset();
    LayoutBoxModelObject::addLayerHitTestRects(layerRects, currentLayer, adjustedLayerOffset, containerRect);
}

void LayoutBox::computeSelfHitTestRects(Vector<LayoutRect>& rects, const LayoutPoint& layerOffset) const
{
    if (!size().isEmpty())
        rects.append(LayoutRect(layerOffset, size()));
}

int LayoutBox::reflectionOffset() const
{
    if (!style()->boxReflect())
        return 0;
    if (style()->boxReflect()->direction() == ReflectionLeft || style()->boxReflect()->direction() == ReflectionRight)
        return valueForLength(style()->boxReflect()->offset(), borderBoxRect().width());
    return valueForLength(style()->boxReflect()->offset(), borderBoxRect().height());
}

LayoutRect LayoutBox::reflectedRect(const LayoutRect& r) const
{
    if (!style()->boxReflect())
        return LayoutRect();

    LayoutRect box = borderBoxRect();
    LayoutRect result = r;
    switch (style()->boxReflect()->direction()) {
    case ReflectionBelow:
        result.setY(box.maxY() + reflectionOffset() + (box.maxY() - r.maxY()));
        break;
    case ReflectionAbove:
        result.setY(box.y() - reflectionOffset() - box.height() + (box.maxY() - r.maxY()));
        break;
    case ReflectionLeft:
        result.setX(box.x() - reflectionOffset() - box.width() + (box.maxX() - r.maxX()));
        break;
    case ReflectionRight:
        result.setX(box.maxX() + reflectionOffset() + (box.maxX() - r.maxX()));
        break;
    }
    return result;
}

int LayoutBox::verticalScrollbarWidth() const
{
    if (!hasOverflowClip() || style()->overflowY() == OverflowOverlay)
        return 0;

    return getScrollableArea()->verticalScrollbarWidth();
}

int LayoutBox::horizontalScrollbarHeight() const
{
    if (!hasOverflowClip() || style()->overflowX() == OverflowOverlay)
        return 0;

    return getScrollableArea()->horizontalScrollbarHeight();
}

ScrollResult LayoutBox::scroll(ScrollGranularity granularity, const FloatSize& delta)
{
    // Presumably the same issue as in setScrollTop. See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    if (!getScrollableArea())
        return ScrollResult();

    return getScrollableArea()->userScroll(granularity, delta);
}

bool LayoutBox::canBeScrolledAndHasScrollableArea() const
{
    return canBeProgramaticallyScrolled() && (pixelSnappedScrollHeight() != pixelSnappedClientHeight() || pixelSnappedScrollWidth() != pixelSnappedClientWidth());
}

bool LayoutBox::canBeProgramaticallyScrolled() const
{
    Node* node = this->node();
    if (node && node->isDocumentNode())
        return true;

    if (!hasOverflowClip())
        return false;

    bool hasScrollableOverflow = hasScrollableOverflowX() || hasScrollableOverflowY();
    if (scrollsOverflow() && hasScrollableOverflow)
        return true;

    return node && node->hasEditableStyle();
}

void LayoutBox::autoscroll(const IntPoint& positionInRootFrame)
{
    LocalFrame* frame = this->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    IntPoint positionInContent = frameView->rootFrameToContents(positionInRootFrame);
    scrollRectToVisible(LayoutRect(positionInContent, LayoutSize(1, 1)), ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, UserScroll);
}

// There are two kinds of layoutObject that can autoscroll.
bool LayoutBox::canAutoscroll() const
{
    if (node() && node()->isDocumentNode())
        return view()->frameView()->isScrollable();

    // Check for a box that can be scrolled in its own right.
    return canBeScrolledAndHasScrollableArea();
}

// If specified point is in border belt, returned offset denotes direction of
// scrolling.
IntSize LayoutBox::calculateAutoscrollDirection(const IntPoint& pointInRootFrame) const
{
    if (!frame())
        return IntSize();

    FrameView* frameView = frame()->view();
    if (!frameView)
        return IntSize();

    IntRect box(absoluteBoundingBoxRect());
    box.move(view()->frameView()->scrollOffset());
    IntRect windowBox = view()->frameView()->contentsToRootFrame(box);

    IntPoint windowAutoscrollPoint = pointInRootFrame;

    if (windowAutoscrollPoint.x() < windowBox.x() + autoscrollBeltSize)
        windowAutoscrollPoint.move(-autoscrollBeltSize, 0);
    else if (windowAutoscrollPoint.x() > windowBox.maxX() - autoscrollBeltSize)
        windowAutoscrollPoint.move(autoscrollBeltSize, 0);

    if (windowAutoscrollPoint.y() < windowBox.y() + autoscrollBeltSize)
        windowAutoscrollPoint.move(0, -autoscrollBeltSize);
    else if (windowAutoscrollPoint.y() > windowBox.maxY() - autoscrollBeltSize)
        windowAutoscrollPoint.move(0, autoscrollBeltSize);

    return windowAutoscrollPoint - pointInRootFrame;
}

LayoutBox* LayoutBox::findAutoscrollable(LayoutObject* layoutObject)
{
    while (layoutObject && !(layoutObject->isBox() && toLayoutBox(layoutObject)->canAutoscroll())) {
        if (!layoutObject->parent() && layoutObject->node() == layoutObject->document() && layoutObject->document().localOwner())
            layoutObject = layoutObject->document().localOwner()->layoutObject();
        else
            layoutObject = layoutObject->parent();
    }

    return layoutObject && layoutObject->isBox() ? toLayoutBox(layoutObject) : 0;
}

static inline int adjustedScrollDelta(int beginningDelta)
{
    // This implemention matches Firefox's.
    // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
    const int speedReducer = 12;

    int adjustedDelta = beginningDelta / speedReducer;
    if (adjustedDelta > 1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(adjustedDelta))) - 1;
    else if (adjustedDelta < -1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(-adjustedDelta))) + 1;

    return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta)
{
    return IntSize(adjustedScrollDelta(delta.width()), adjustedScrollDelta(delta.height()));
}

void LayoutBox::panScroll(const IntPoint& sourcePoint)
{
    LocalFrame* frame = this->frame();
    if (!frame)
        return;

    IntPoint lastKnownMousePosition = frame->eventHandler().lastKnownMousePosition();

    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    IntSize delta = lastKnownMousePosition - sourcePoint;

    if (abs(delta.width()) <= AutoscrollController::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= AutoscrollController::noPanScrollRadius)
        delta.setHeight(0);
    scroll(ScrollByPixel, FloatSize(adjustedScrollDelta(delta)));
}

void LayoutBox::scrollByRecursively(const DoubleSize& delta, ScrollOffsetClamping clamp)
{
    if (delta.isZero())
        return;

    bool restrictedByLineClamp = false;
    if (parent())
        restrictedByLineClamp = !parent()->style()->lineClamp().isNone();

    if (hasOverflowClip() && !restrictedByLineClamp) {
        PaintLayerScrollableArea* scrollableArea = this->getScrollableArea();
        ASSERT(scrollableArea);

        DoubleSize newScrollOffset = scrollableArea->adjustedScrollOffset() + delta;
        scrollableArea->scrollToOffset(newScrollOffset, clamp);

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        DoubleSize remainingScrollOffset = newScrollOffset - scrollableArea->adjustedScrollOffset();
        if (!remainingScrollOffset.isZero() && parent()) {
            if (LayoutBox* scrollableBox = enclosingScrollableBox())
                scrollableBox->scrollByRecursively(remainingScrollOffset, clamp);

            LocalFrame* frame = this->frame();
            if (frame && frame->page())
                frame->page()->autoscrollController().updateAutoscrollLayoutObject();
        }
    } else if (view()->frameView()) {
        // If we are here, we were called on a layoutObject that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        // FIXME: Pass in DoubleSize. crbug.com/414283.
        view()->frameView()->scrollBy(flooredIntSize(delta), UserScroll);

        // FIXME: If we didn't scroll the whole way, do we want to try looking at the frames ownerElement?
        // https://bugs.webkit.org/show_bug.cgi?id=28237
    }
}

bool LayoutBox::needsPreferredWidthsRecalculation() const
{
    return style()->paddingStart().hasPercent() || style()->paddingEnd().hasPercent();
}

IntSize LayoutBox::originAdjustmentForScrollbars() const
{
    IntSize size;
    int adjustmentWidth = verticalScrollbarWidth();
    if (hasFlippedBlocksWritingMode()
        || (isHorizontalWritingMode() && shouldPlaceBlockDirectionScrollbarOnLogicalLeft())) {
        size.expand(adjustmentWidth, 0);
    }
    return size;
}

IntSize LayoutBox::scrolledContentOffset() const
{
    ASSERT(hasOverflowClip());
    ASSERT(hasLayer());
    // FIXME: Return DoubleSize here. crbug.com/414283.
    PaintLayerScrollableArea* scrollableArea = getScrollableArea();
    IntSize result = flooredIntSize(scrollableArea->scrollOffset()) + originAdjustmentForScrollbars();
    if (isHorizontalWritingMode() && shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        result.expand(-verticalScrollbarWidth(), 0);
    return result;
}

bool LayoutBox::mapScrollingContentsRectToBoxSpace(LayoutRect& rect, ApplyOverflowClipFlag applyOverflowClip, VisualRectFlags visualRectFlags) const
{
    if (!hasOverflowClip())
        return true;

    LayoutSize offset = LayoutSize(-scrolledContentOffset());
    if (UNLIKELY(hasFlippedBlocksWritingMode()))
        offset.setWidth(-offset.width());
    rect.move(offset);

    if (applyOverflowClip == ApplyNonScrollOverflowClip && scrollsOverflow())
        return true;

    flipForWritingMode(rect);

    LayoutRect clipRect = overflowClipRect(LayoutPoint());

    bool doesIntersect;
    if (visualRectFlags & EdgeInclusive) {
        doesIntersect = rect.inclusiveIntersect(clipRect);
    } else {
        rect.intersect(clipRect);
        doesIntersect = !rect.isEmpty();
    }
    if (doesIntersect)
        flipForWritingMode(rect);
    return doesIntersect;
}

void LayoutBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    minLogicalWidth = minPreferredLogicalWidth() - borderAndPaddingLogicalWidth();
    maxLogicalWidth = maxPreferredLogicalWidth() - borderAndPaddingLogicalWidth();
}

LayoutUnit LayoutBox::minPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty()) {
#if ENABLE(ASSERT)
        SetLayoutNeededForbiddenScope layoutForbiddenScope(const_cast<LayoutBox&>(*this));
#endif
        const_cast<LayoutBox*>(this)->computePreferredLogicalWidths();
        ASSERT(!preferredLogicalWidthsDirty());
    }

    return m_minPreferredLogicalWidth;
}

LayoutUnit LayoutBox::maxPreferredLogicalWidth() const
{
    if (preferredLogicalWidthsDirty()) {
#if ENABLE(ASSERT)
        SetLayoutNeededForbiddenScope layoutForbiddenScope(const_cast<LayoutBox&>(*this));
#endif
        const_cast<LayoutBox*>(this)->computePreferredLogicalWidths();
        ASSERT(!preferredLogicalWidthsDirty());
    }

    return m_maxPreferredLogicalWidth;
}

bool LayoutBox::hasOverrideLogicalContentHeight() const
{
    return m_rareData && m_rareData->m_overrideLogicalContentHeight != -1;
}

bool LayoutBox::hasOverrideLogicalContentWidth() const
{
    return m_rareData && m_rareData->m_overrideLogicalContentWidth != -1;
}

void LayoutBox::setOverrideLogicalContentHeight(LayoutUnit height)
{
    ASSERT(height >= 0);
    ensureRareData().m_overrideLogicalContentHeight = height;
}

void LayoutBox::setOverrideLogicalContentWidth(LayoutUnit width)
{
    ASSERT(width >= 0);
    ensureRareData().m_overrideLogicalContentWidth = width;
}

void LayoutBox::clearOverrideLogicalContentHeight()
{
    if (m_rareData)
        m_rareData->m_overrideLogicalContentHeight = LayoutUnit(-1);
}

void LayoutBox::clearOverrideLogicalContentWidth()
{
    if (m_rareData)
        m_rareData->m_overrideLogicalContentWidth = LayoutUnit(-1);
}

void LayoutBox::clearOverrideSize()
{
    clearOverrideLogicalContentHeight();
    clearOverrideLogicalContentWidth();
}

LayoutUnit LayoutBox::overrideLogicalContentWidth() const
{
    ASSERT(hasOverrideLogicalContentWidth());
    return m_rareData->m_overrideLogicalContentWidth;
}

LayoutUnit LayoutBox::overrideLogicalContentHeight() const
{
    ASSERT(hasOverrideLogicalContentHeight());
    return m_rareData->m_overrideLogicalContentHeight;
}

LayoutUnit LayoutBox::overrideContainingBlockContentLogicalWidth() const
{
    ASSERT(hasOverrideContainingBlockLogicalWidth());
    return gOverrideContainingBlockLogicalWidthMap->get(this);
}

LayoutUnit LayoutBox::overrideContainingBlockContentLogicalHeight() const
{
    ASSERT(hasOverrideContainingBlockLogicalHeight());
    return gOverrideContainingBlockLogicalHeightMap->get(this);
}

bool LayoutBox::hasOverrideContainingBlockLogicalWidth() const
{
    return gOverrideContainingBlockLogicalWidthMap && gOverrideContainingBlockLogicalWidthMap->contains(this);
}

bool LayoutBox::hasOverrideContainingBlockLogicalHeight() const
{
    return gOverrideContainingBlockLogicalHeightMap && gOverrideContainingBlockLogicalHeightMap->contains(this);
}

void LayoutBox::setOverrideContainingBlockContentLogicalWidth(LayoutUnit logicalWidth)
{
    if (!gOverrideContainingBlockLogicalWidthMap)
        gOverrideContainingBlockLogicalWidthMap = new OverrideSizeMap;
    gOverrideContainingBlockLogicalWidthMap->set(this, logicalWidth);
}

void LayoutBox::setOverrideContainingBlockContentLogicalHeight(LayoutUnit logicalHeight)
{
    if (!gOverrideContainingBlockLogicalHeightMap)
        gOverrideContainingBlockLogicalHeightMap = new OverrideSizeMap;
    gOverrideContainingBlockLogicalHeightMap->set(this, logicalHeight);
}

void LayoutBox::clearContainingBlockOverrideSize()
{
    if (gOverrideContainingBlockLogicalWidthMap)
        gOverrideContainingBlockLogicalWidthMap->remove(this);
    clearOverrideContainingBlockContentLogicalHeight();
}

void LayoutBox::clearOverrideContainingBlockContentLogicalHeight()
{
    if (gOverrideContainingBlockLogicalHeightMap)
        gOverrideContainingBlockLogicalHeightMap->remove(this);
}

LayoutUnit LayoutBox::extraInlineOffset() const
{
    return gExtraInlineOffsetMap ? gExtraInlineOffsetMap->get(this) : LayoutUnit();
}

LayoutUnit LayoutBox::extraBlockOffset() const
{
    return gExtraBlockOffsetMap ? gExtraBlockOffsetMap->get(this) : LayoutUnit();
}

void LayoutBox::setExtraInlineOffset(LayoutUnit inlineOffest)
{
    if (!gExtraInlineOffsetMap)
        gExtraInlineOffsetMap = new OverrideSizeMap;
    gExtraInlineOffsetMap->set(this, inlineOffest);
}

void LayoutBox::setExtraBlockOffset(LayoutUnit blockOffest)
{
    if (!gExtraBlockOffsetMap)
        gExtraBlockOffsetMap = new OverrideSizeMap;
    gExtraBlockOffsetMap->set(this, blockOffest);
}

void LayoutBox::clearExtraInlineAndBlockOffests()
{
    if (gExtraInlineOffsetMap)
        gExtraInlineOffsetMap->remove(this);
    if (gExtraBlockOffsetMap)
        gExtraBlockOffsetMap->remove(this);
}

LayoutUnit LayoutBox::adjustBorderBoxLogicalWidthForBoxSizing(float width) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    LayoutUnit result(width);
    if (style()->boxSizing() == BoxSizingContentBox)
        return result + bordersPlusPadding;
    return std::max(result, bordersPlusPadding);
}

LayoutUnit LayoutBox::adjustBorderBoxLogicalHeightForBoxSizing(float height) const
{
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    LayoutUnit result(height);
    if (style()->boxSizing() == BoxSizingContentBox)
        return result + bordersPlusPadding;
    return std::max(result, bordersPlusPadding);
}

LayoutUnit LayoutBox::adjustContentBoxLogicalWidthForBoxSizing(float width) const
{
    LayoutUnit result(width);
    if (style()->boxSizing() == BoxSizingBorderBox)
        result -= borderAndPaddingLogicalWidth();
    return std::max(LayoutUnit(), result);
}

LayoutUnit LayoutBox::adjustContentBoxLogicalHeightForBoxSizing(float height) const
{
    LayoutUnit result(height);
    if (style()->boxSizing() == BoxSizingBorderBox)
        result -= borderAndPaddingLogicalHeight();
    return std::max(LayoutUnit(), result);
}

// Hit Testing
bool LayoutBox::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    if (!isLayoutView()) {
        // Check if we need to do anything at all.
        // If we have clipping, then we can't have any spillout.
        LayoutRect overflowBox = hasOverflowClip() ? borderBoxRect() : visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedLocation);
        if (!locationInContainer.intersects(overflowBox))
            return false;
    }

    bool shouldHitTestSelf = isInSelfHitTestingPhase(action);

    if (shouldHitTestSelf && hasOverflowClip()
        && hitTestOverflowControl(result, locationInContainer, adjustedLocation))
        return true;

    // TODO(pdr): We should also check for css clip in the !isSelfPaintingLayer
    //            case, similar to overflow clip below.
    bool skipChildren = false;
    if (hasOverflowClip() && !hasSelfPaintingLayer()) {
        if (!locationInContainer.intersects(overflowClipRect(adjustedLocation, ExcludeOverlayScrollbarSizeForHitTesting))) {
            skipChildren = true;
        } else if (style()->hasBorderRadius()) {
            LayoutRect boundsRect(adjustedLocation, size());
            skipChildren = !locationInContainer.intersects(style()->getRoundedInnerBorderFor(boundsRect));
        }
    }

    // A control clip can also clip out child hit testing.
    if (!skipChildren && hasControlClip() && !locationInContainer.intersects(controlClipRect(adjustedLocation)))
        skipChildren = true;

    // TODO(pdr): We should also include checks for hit testing border radius at
    //            the layer level (see: crbug.com/568904).

    if (!skipChildren && hitTestChildren(result, locationInContainer, adjustedLocation, action))
        return true;

    if (hitTestClippedOutByRoundedBorder(locationInContainer, adjustedLocation))
        return false;

    // Now hit test ourselves.
    if (shouldHitTestSelf && visibleToHitTestRequest(result.hitTestRequest())) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(adjustedLocation)));
            if (result.addNodeToListBasedTestResult(nodeForHitTest(), locationInContainer, boundsRect) == StopHitTesting)
                return true;
        }
    }

    return false;
}

bool LayoutBox::hitTestChildren(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    for (LayoutObject* child = slowLastChild(); child; child = child->previousSibling()) {
        if ((!child->hasLayer() || !toLayoutBoxModelObject(child)->layer()->isSelfPaintingLayer()) && child->nodeAtPoint(result, locationInContainer, accumulatedOffset, action))
            return true;
    }

    return false;
}

bool LayoutBox::hitTestClippedOutByRoundedBorder(const HitTestLocation& locationInContainer, const LayoutPoint& borderBoxLocation) const
{
    if (!style()->hasBorderRadius())
        return false;

    LayoutRect borderRect = borderBoxRect();
    borderRect.moveBy(borderBoxLocation);
    return !locationInContainer.intersects(style()->getRoundedBorderFor(borderRect));
}

void LayoutBox::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BoxPainter(*this).paint(paintInfo, paintOffset);
}

void LayoutBox::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BoxPainter(*this).paintBoxDecorationBackground(paintInfo, paintOffset);
}

bool LayoutBox::getBackgroundPaintedExtent(LayoutRect& paintedExtent) const
{
    ASSERT(hasBackground());

    // LayoutView is special in the sense that it expands to the whole canvas,
    // thus can't be handled by this function.
    ASSERT(!isLayoutView());

    LayoutRect backgroundRect(borderBoxRect());

    Color backgroundColor = resolveColor(CSSPropertyBackgroundColor);
    if (backgroundColor.alpha()) {
        paintedExtent = backgroundRect;
        return true;
    }

    if (!style()->backgroundLayers().image() || style()->backgroundLayers().next()) {
        paintedExtent =  backgroundRect;
        return true;
    }

    BackgroundImageGeometry geometry;
    // TODO(jchaffraix): This function should be rethought as it's called during and outside
    // of the paint phase. Potentially returning different results at different phases.
    geometry.calculate(*this, nullptr, GlobalPaintNormalPhase, style()->backgroundLayers(), backgroundRect);
    if (geometry.hasNonLocalGeometry())
        return false;
    paintedExtent = LayoutRect(geometry.destRect());
    return true;
}

bool LayoutBox::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (isDocumentElement() || backgroundStolenForBeingBody())
        return false;

    Color backgroundColor = resolveColor(CSSPropertyBackgroundColor);
    if (backgroundColor.hasAlpha())
        return false;

    // If the element has appearance, it might be painted by theme.
    // We cannot be sure if theme paints the background opaque.
    // In this case it is safe to not assume opaqueness.
    // FIXME: May be ask theme if it paints opaque.
    if (style()->hasAppearance())
        return false;
    // FIXME: Check the opaqueness of background images.

    // FIXME: Use rounded rect if border radius is present.
    if (style()->hasBorderRadius())
        return false;
    if (hasClipPath())
        return false;
    // FIXME: The background color clip is defined by the last layer.
    if (style()->backgroundLayers().next())
        return false;
    LayoutRect backgroundRect;
    switch (style()->backgroundClip()) {
    case BorderFillBox:
        backgroundRect = borderBoxRect();
        break;
    case PaddingFillBox:
        backgroundRect = paddingBoxRect();
        break;
    case ContentFillBox:
        backgroundRect = contentBoxRect();
        break;
    default:
        break;
    }
    return backgroundRect.contains(localRect);
}

static bool isCandidateForOpaquenessTest(const LayoutBox& childBox)
{
    const ComputedStyle& childStyle = childBox.styleRef();
    if (childStyle.position() != StaticPosition && childBox.containingBlock() != childBox.parent())
        return false;
    if (childStyle.visibility() != VISIBLE || childStyle.shapeOutside())
        return false;
    if (childBox.size().isZero())
        return false;
    if (PaintLayer* childLayer = childBox.layer()) {
        // FIXME: perhaps this could be less conservative?
        if (childLayer->compositingState() != NotComposited)
            return false;
        // FIXME: Deal with z-index.
        if (!childStyle.hasAutoZIndex())
            return false;
        if (childLayer->hasTransformRelatedProperty() || childLayer->isTransparent() || childLayer->hasFilterInducingProperty())
            return false;
        if (childBox.hasOverflowClip() && childStyle.hasBorderRadius())
            return false;
    }
    return true;
}

bool LayoutBox::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    if (!maxDepthToTest)
        return false;
    for (LayoutObject* child = slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->isBox())
            continue;
        LayoutBox* childBox = toLayoutBox(child);
        if (!isCandidateForOpaquenessTest(*childBox))
            continue;
        LayoutPoint childLocation = childBox->location();
        if (childBox->isInFlowPositioned())
            childLocation.move(childBox->offsetForInFlowPosition());
        LayoutRect childLocalRect = localRect;
        childLocalRect.moveBy(-childLocation);
        if (childLocalRect.y() < 0 || childLocalRect.x() < 0) {
            // If there is unobscured area above/left of a static positioned box then the rect is probably not covered.
            if (!childBox->isPositioned())
                return false;
            continue;
        }
        if (childLocalRect.maxY() > childBox->size().height() || childLocalRect.maxX() > childBox->size().width())
            continue;
        if (childBox->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
        if (childBox->foregroundIsKnownToBeOpaqueInRect(childLocalRect, maxDepthToTest - 1))
            return true;
    }
    return false;
}

bool LayoutBox::computeBackgroundIsKnownToBeObscured() const
{
    if (scrollsOverflow())
        return false;
    // Test to see if the children trivially obscure the background.
    // FIXME: This test can be much more comprehensive.
    if (!hasBackground())
        return false;
    // Root background painting is special.
    if (isLayoutView())
        return false;
    // FIXME: box-shadow is painted while background painting.
    if (style()->boxShadow())
        return false;
    LayoutRect backgroundRect;
    if (!getBackgroundPaintedExtent(backgroundRect))
        return false;
    return foregroundIsKnownToBeOpaqueInRect(backgroundRect, backgroundObscurationTestMaxDepth);
}

void LayoutBox::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    BoxPainter(*this).paintMask(paintInfo, paintOffset);
}

void LayoutBox::imageChanged(WrappedImagePtr image, const IntRect*)
{
    // TODO(chrishtr): support PaintInvalidationDelayedFull for animated border images.
    if ((style()->borderImage().image() && style()->borderImage().image()->data() == image)
        || (style()->maskBoxImage().image() && style()->maskBoxImage().image()->data() == image)) {
        setShouldDoFullPaintInvalidation();
        return;
    }

    ShapeValue* shapeOutsideValue = style()->shapeOutside();
    if (!frameView()->isInPerformLayout() && isFloating() && shapeOutsideValue && shapeOutsideValue->image() && shapeOutsideValue->image()->data() == image) {
        ShapeOutsideInfo& info = ShapeOutsideInfo::ensureInfo(*this);
        if (!info.isComputingShape()) {
            info.markShapeAsDirty();
            markShapeOutsideDependentsForLayout();
        }
    }

    if (!invalidatePaintOfLayerRectsForImage(image, style()->backgroundLayers(), true))
        invalidatePaintOfLayerRectsForImage(image, style()->maskLayers(), false);
}

ResourcePriority LayoutBox::computeResourcePriority() const
{
    LayoutRect viewBounds = viewRect();
    LayoutRect objectBounds = LayoutRect(absoluteContentBox());

    // The object bounds might be empty right now, so intersects will fail since it doesn't deal
    // with empty rects. Use LayoutRect::contains in that case.
    bool isVisible;
    if (!objectBounds.isEmpty())
        isVisible =  viewBounds.intersects(objectBounds);
    else
        isVisible = viewBounds.contains(objectBounds);

    LayoutRect screenRect;
    if (!objectBounds.isEmpty()) {
        screenRect = viewBounds;
        screenRect.intersect(objectBounds);
    }

    int screenArea = 0;
    if (!screenRect.isEmpty() && isVisible)
        screenArea = static_cast<uint32_t>(screenRect.width() * screenRect.height());
    return ResourcePriority(isVisible ? ResourcePriority::Visible : ResourcePriority::NotVisible, screenArea);
}

bool LayoutBox::invalidatePaintOfLayerRectsForImage(WrappedImagePtr image, const FillLayer& layers, bool drawingBackground)
{
    if (drawingBackground && (isDocumentElement() || backgroundStolenForBeingBody()))
        return false;
    for (const FillLayer* curLayer = &layers; curLayer; curLayer = curLayer->next()) {
        if (curLayer->image() && image == curLayer->image()->data()) {
            bool maybeAnimated = curLayer->image()->cachedImage() && curLayer->image()->cachedImage()->getImage() && curLayer->image()->cachedImage()->getImage()->maybeAnimated();
            if (maybeAnimated && drawingBackground)
                setShouldDoFullPaintInvalidation(PaintInvalidationDelayedFull);
            else
                setShouldDoFullPaintInvalidation();

            if (drawingBackground)
                invalidateBackgroundObscurationStatus();
            return true;
        }
    }
    return false;
}

bool LayoutBox::intersectsVisibleViewport()
{
    LayoutRect rect = visualOverflowRect();
    LayoutView* layoutView = view();
    while (layoutView->frame()->ownerLayoutObject())
        layoutView = layoutView->frame()->ownerLayoutObject()->view();
    mapToVisualRectInAncestorSpace(layoutView, rect);
    return rect.intersects(LayoutRect(layoutView->frameView()->getScrollableArea()->visibleContentRectDouble()));
}

PaintInvalidationReason LayoutBox::invalidatePaintIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    if (hasBoxDecorationBackground()
        // We also paint overflow controls in background phase.
        || (hasOverflowClip() && getScrollableArea()->hasOverflowControls())) {
        PaintLayer& layer = paintInvalidationState.paintingLayer();
        if (layer.layoutObject() != this)
            layer.setNeedsPaintPhaseDescendantBlockBackgrounds();
    }

    PaintInvalidationReason fullInvalidationReason = fullPaintInvalidationReason();
    // If the current paint invalidation reason is PaintInvalidationDelayedFull, then this paint invalidation can delayed if the
    // LayoutBox in question is not on-screen. The logic to decide whether this is appropriate exists at the site of the original
    // paint invalidation that chose PaintInvalidationDelayedFull.
    if (fullInvalidationReason == PaintInvalidationDelayedFull) {
        if (!intersectsVisibleViewport())
            return PaintInvalidationDelayedFull;

        // Reset state back to regular full paint invalidation if the object is onscreen.
        setShouldDoFullPaintInvalidation(PaintInvalidationFull);
    }

    PaintInvalidationReason reason = LayoutBoxModelObject::invalidatePaintIfNeeded(paintInvalidationState);

    if (PaintLayerScrollableArea* area = getScrollableArea())
        area->invalidatePaintOfScrollControlsIfNeeded(paintInvalidationState);

    // This is for the next invalidatePaintIfNeeded so must be at the end.
    savePreviousBoxSizesIfNeeded();
    return reason;
}

void LayoutBox::invalidatePaintOfSubtreesIfNeeded(const PaintInvalidationState& childPaintInvalidationState)
{
    LayoutBoxModelObject::invalidatePaintOfSubtreesIfNeeded(childPaintInvalidationState);

    if (PaintLayer* layer = this->layer()) {
        if (PaintLayerReflectionInfo* reflectionInfo = layer->reflectionInfo())
            reflectionInfo->reflection()->invalidateTreeIfNeeded(childPaintInvalidationState);
    }
}

LayoutRect LayoutBox::overflowClipRect(const LayoutPoint& location, OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const
{
    // FIXME: When overflow-clip (CSS3) is implemented, we'll obtain the property
    // here.
    LayoutRect clipRect = borderBoxRect();
    clipRect.setLocation(location + clipRect.location() + LayoutSize(borderLeft(), borderTop()));
    clipRect.setSize(clipRect.size() - LayoutSize(borderLeft() + borderRight(), borderTop() + borderBottom()));

    if (hasOverflowClip())
        excludeScrollbars(clipRect, overlayScrollbarClipBehavior);
    return clipRect;
}

void LayoutBox::excludeScrollbars(LayoutRect& rect, OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const
{
    if (PaintLayerScrollableArea* scrollableArea = this->getScrollableArea()) {
        if (shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
            rect.move(scrollableArea->verticalScrollbarWidth(overlayScrollbarClipBehavior), 0);
        rect.contract(scrollableArea->verticalScrollbarWidth(overlayScrollbarClipBehavior), scrollableArea->horizontalScrollbarHeight(overlayScrollbarClipBehavior));
    }
}

LayoutRect LayoutBox::clipRect(const LayoutPoint& location) const
{
    LayoutRect borderBoxRect = this->borderBoxRect();
    LayoutRect clipRect = LayoutRect(borderBoxRect.location() + location, borderBoxRect.size());

    if (!style()->clipLeft().isAuto()) {
        LayoutUnit c = valueForLength(style()->clipLeft(), borderBoxRect.width());
        clipRect.move(c, LayoutUnit());
        clipRect.contract(c, LayoutUnit());
    }

    if (!style()->clipRight().isAuto())
        clipRect.contract(size().width() - valueForLength(style()->clipRight(), size().width()), LayoutUnit());

    if (!style()->clipTop().isAuto()) {
        LayoutUnit c = valueForLength(style()->clipTop(), borderBoxRect.height());
        clipRect.move(LayoutUnit(), c);
        clipRect.contract(LayoutUnit(), c);
    }

    if (!style()->clipBottom().isAuto())
        clipRect.contract(LayoutUnit(), size().height() - valueForLength(style()->clipBottom(), size().height()));

    return clipRect;
}

static LayoutUnit portionOfMarginNotConsumedByFloat(LayoutUnit childMargin, LayoutUnit contentSide, LayoutUnit offset)
{
    if (childMargin <= 0)
        return LayoutUnit();
    LayoutUnit contentSideWithMargin = contentSide + childMargin;
    if (offset > contentSideWithMargin)
        return childMargin;
    return offset - contentSide;
}

LayoutUnit LayoutBox::shrinkLogicalWidthToAvoidFloats(LayoutUnit childMarginStart, LayoutUnit childMarginEnd, const LayoutBlockFlow* cb) const
{
    LayoutUnit logicalTopPosition = logicalTop();
    LayoutUnit startOffsetForContent = cb->startOffsetForContent();
    LayoutUnit endOffsetForContent = cb->endOffsetForContent();
    LayoutUnit startOffsetForLine = cb->startOffsetForLine(logicalTopPosition, DoNotIndentText);
    LayoutUnit endOffsetForLine = cb->endOffsetForLine(logicalTopPosition, DoNotIndentText);

    // If there aren't any floats constraining us then allow the margins to shrink/expand the width as much as they want.
    if (startOffsetForContent == startOffsetForLine && endOffsetForContent == endOffsetForLine)
        return cb->availableLogicalWidthForLine(logicalTopPosition, DoNotIndentText) - childMarginStart - childMarginEnd;

    LayoutUnit width = cb->availableLogicalWidthForLine(logicalTopPosition, DoNotIndentText) - std::max(LayoutUnit(), childMarginStart) - std::max(LayoutUnit(), childMarginEnd);
    // We need to see if margins on either the start side or the end side can contain the floats in question. If they can,
    // then just using the line width is inaccurate. In the case where a float completely fits, we don't need to use the line
    // offset at all, but can instead push all the way to the content edge of the containing block. In the case where the float
    // doesn't fit, we can use the line offset, but we need to grow it by the margin to reflect the fact that the margin was
    // "consumed" by the float. Negative margins aren't consumed by the float, and so we ignore them.
    width += portionOfMarginNotConsumedByFloat(childMarginStart, startOffsetForContent, startOffsetForLine);
    width += portionOfMarginNotConsumedByFloat(childMarginEnd, endOffsetForContent, endOffsetForLine);
    return width;
}

LayoutUnit LayoutBox::containingBlockLogicalHeightForGetComputedStyle() const
{
    if (hasOverrideContainingBlockLogicalHeight())
        return overrideContainingBlockContentLogicalHeight();

    if (!isPositioned())
        return containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);

    LayoutBoxModelObject* cb = toLayoutBoxModelObject(container());
    LayoutUnit height = containingBlockLogicalHeightForPositioned(cb);
    if (styleRef().position() != AbsolutePosition)
        height -= cb->paddingLogicalHeight();
    return height;
}

LayoutUnit LayoutBox::containingBlockLogicalWidthForContent() const
{
    if (hasOverrideContainingBlockLogicalWidth())
        return overrideContainingBlockContentLogicalWidth();

    LayoutBlock* cb = containingBlock();
    if (isOutOfFlowPositioned())
        return cb->clientLogicalWidth();
    return cb->availableLogicalWidth();
}

LayoutUnit LayoutBox::containingBlockLogicalHeightForContent(AvailableLogicalHeightType heightType) const
{
    if (hasOverrideContainingBlockLogicalHeight())
        return overrideContainingBlockContentLogicalHeight();

    LayoutBlock* cb = containingBlock();
    return cb->availableLogicalHeight(heightType);
}

LayoutUnit LayoutBox::containingBlockAvailableLineWidth() const
{
    LayoutBlock* cb = containingBlock();
    if (cb->isLayoutBlockFlow())
        return toLayoutBlockFlow(cb)->availableLogicalWidthForLine(logicalTop(), DoNotIndentText, availableLogicalHeight(IncludeMarginBorderPadding));
    return LayoutUnit();
}

LayoutUnit LayoutBox::perpendicularContainingBlockLogicalHeight() const
{
    if (hasOverrideContainingBlockLogicalHeight())
        return overrideContainingBlockContentLogicalHeight();

    LayoutBlock* cb = containingBlock();
    if (cb->hasOverrideLogicalContentHeight())
        return cb->overrideLogicalContentHeight();

    const ComputedStyle& containingBlockStyle = cb->styleRef();
    Length logicalHeightLength = containingBlockStyle.logicalHeight();

    // FIXME: For now just support fixed heights.  Eventually should support percentage heights as well.
    if (!logicalHeightLength.isFixed()) {
        LayoutUnit fillFallbackExtent = LayoutUnit(containingBlockStyle.isHorizontalWritingMode()
            ? view()->frameView()->visibleContentSize().height()
            : view()->frameView()->visibleContentSize().width());
        LayoutUnit fillAvailableExtent = containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding);
        if (fillAvailableExtent == -1)
            return fillFallbackExtent;
        return std::min(fillAvailableExtent, fillFallbackExtent);
    }

    // Use the content box logical height as specified by the style.
    return cb->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit(logicalHeightLength.value()));
}

void LayoutBox::mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags mode) const
{
    bool isFixedPos = style()->position() == FixedPosition;

    // If this box has a transform or contains paint, it acts as a fixed position container for fixed descendants,
    // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
    if (style()->canContainFixedPositionObjects() && !isFixedPos)
        mode &= ~IsFixed;
    else if (isFixedPos)
        mode |= IsFixed;

    LayoutBoxModelObject::mapLocalToAncestor(ancestor, transformState, mode);
}

void LayoutBox::mapAncestorToLocal(const LayoutBoxModelObject* ancestor, TransformState& transformState, MapCoordinatesFlags mode) const
{
    if (this == ancestor)
        return;

    bool isFixedPos = style()->position() == FixedPosition;

    if (style()->canContainFixedPositionObjects() && !isFixedPos) {
        // If this box has a transform or contains paint, it acts as a fixed position container for fixed descendants,
        // and may itself also be fixed position. So propagate 'fixed' up only if this box is fixed position.
        mode &= ~IsFixed;
    } else if (isFixedPos) {
        mode |= IsFixed;
    }

    LayoutBoxModelObject::mapAncestorToLocal(ancestor, transformState, mode);
}

LayoutSize LayoutBox::offsetFromContainer(const LayoutObject* o) const
{
    ASSERT(o == container());

    LayoutSize offset;
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    offset += topLeftLocationOffset();

    if (o->hasOverflowClip())
        offset -= toLayoutBox(o)->scrolledContentOffset();

    if (style()->position() == AbsolutePosition && o->isInFlowPositioned() && o->isLayoutInline())
        offset += toLayoutInline(o)->offsetForInFlowPositionedInline(*this);

    return offset;
}

InlineBox* LayoutBox::createInlineBox()
{
    return new InlineBox(LineLayoutItem(this));
}

void LayoutBox::dirtyLineBoxes(bool fullLayout)
{
    if (m_inlineBoxWrapper) {
        if (fullLayout) {
            m_inlineBoxWrapper->destroy();
            m_inlineBoxWrapper = nullptr;
        } else {
            m_inlineBoxWrapper->dirtyLineBoxes();
        }
    }
}

void LayoutBox::positionLineBox(InlineBox* box)
{
    if (isOutOfFlowPositioned()) {
        // Cache the x position only if we were an INLINE type originally.
        bool originallyInline = style()->isOriginalDisplayInlineType();
        if (originallyInline) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            RootInlineBox& root = box->root();
            root.block().setStaticInlinePositionForChild(LineLayoutBox(this), box->logicalLeft());
        } else {
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the y() of the box.
            layer()->setStaticBlockPosition(box->logicalTop());
        }

        if (container()->isLayoutInline())
            moveWithEdgeOfInlineContainerIfNecessary(box->isHorizontal());

        // Nuke the box.
        box->remove(DontMarkLineBoxes);
        box->destroy();
    } else if (isAtomicInlineLevel()) {
        // FIXME: the call to roundedLayoutPoint() below is temporary and should be removed once
        // the transition to LayoutUnit-based types is complete (crbug.com/321237)
        setLocationAndUpdateOverflowControlsIfNeeded(box->topLeft());
        setInlineBoxWrapper(box);
    }
}

void LayoutBox::moveWithEdgeOfInlineContainerIfNecessary(bool isHorizontal)
{
    ASSERT(isOutOfFlowPositioned() && container()->isLayoutInline() && container()->isInFlowPositioned());
    // If this object is inside a relative positioned inline and its inline position is an explicit offset from the edge of its container
    // then it will need to move if its inline container has changed width. We do not track if the width has changed
    // but if we are here then we are laying out lines inside it, so it probably has - mark our object for layout so that it can
    // move to the new offset created by the new width.
    if (!normalChildNeedsLayout() && !style()->hasStaticInlinePosition(isHorizontal))
        setChildNeedsLayout(MarkOnlyThis);
}

void LayoutBox::deleteLineBoxWrapper()
{
    if (m_inlineBoxWrapper) {
        if (!documentBeingDestroyed())
            m_inlineBoxWrapper->remove();
        m_inlineBoxWrapper->destroy();
        m_inlineBoxWrapper = nullptr;
    }
}

void LayoutBox::setSpannerPlaceholder(LayoutMultiColumnSpannerPlaceholder& placeholder)
{
    RELEASE_ASSERT(!m_rareData || !m_rareData->m_spannerPlaceholder); // not expected to change directly from one spanner to another.
    ensureRareData().m_spannerPlaceholder = &placeholder;
}

void LayoutBox::clearSpannerPlaceholder()
{
    if (!m_rareData)
        return;
    m_rareData->m_spannerPlaceholder = nullptr;
}

void LayoutBox::setPaginationStrut(LayoutUnit strut)
{
    if (!strut && !m_rareData)
        return;
    ensureRareData().m_paginationStrut = strut;
}

bool LayoutBox::isBreakBetweenControllable(EBreak breakValue) const
{
    if (breakValue == BreakAuto)
        return true;
    // We currently only support non-auto break-before and break-after values on in-flow block
    // level elements, which is the minimum requirement according to the spec.
    if (isInline() || isFloatingOrOutOfFlowPositioned())
        return false;
    const LayoutBlock* curr = containingBlock();
    if (!curr || !curr->isLayoutBlockFlow())
        return false;
    const LayoutView* layoutView = view();
    bool viewIsPaginated = layoutView->fragmentationContext();
    if (!viewIsPaginated && !flowThreadContainingBlock())
        return false;
    while (curr) {
        if (curr == layoutView)
            return viewIsPaginated && breakValue != BreakColumn && breakValue != BreakAvoidColumn;
        if (curr->isLayoutFlowThread()) {
            if (breakValue == BreakAvoid) // Valid in any kind of fragmentation context.
                return true;
            bool isMulticolValue = breakValue == BreakColumn || breakValue == BreakAvoidColumn;
            if (toLayoutFlowThread(curr)->isLayoutPagedFlowThread())
                return !isMulticolValue;
            if (isMulticolValue)
                return true;
            // If this is a flow thread for a multicol container, and we have a break value for
            // paged, we need to keep looking.
        }
        if (curr->isFloatingOrOutOfFlowPositioned())
            return false;
        curr = curr->containingBlock();
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool LayoutBox::isBreakInsideControllable(EBreak breakValue) const
{
    ASSERT(!isForcedFragmentainerBreakValue(breakValue));
    if (breakValue == BreakAuto)
        return true;
    // First check multicol.
    const LayoutFlowThread* flowThread = flowThreadContainingBlock();
    // 'avoid-column' is only valid in a multicol context.
    if (breakValue == BreakAvoidColumn)
        return flowThread && !flowThread->isLayoutPagedFlowThread();
    // 'avoid' is valid in any kind of fragmentation context.
    if (breakValue == BreakAvoid && flowThread)
        return true;
    ASSERT(breakValue == BreakAvoidPage || breakValue == BreakAvoid);
    if (view()->fragmentationContext())
        return true; // The view is paginated, probably because we're printing.
    if (!flowThread)
        return false; // We're not inside any pagination context
    // We're inside a flow thread. We need to be contained by a flow thread for paged overflow in
    // order for pagination values to be valid, though.
    for (const LayoutBlock* ancestor = flowThread; ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor->isLayoutFlowThread() && toLayoutFlowThread(ancestor)->isLayoutPagedFlowThread())
            return true;
    }
    return false;
}

EBreak LayoutBox::breakAfter() const
{
    EBreak breakValue = style()->breakAfter();
    if (breakValue == BreakAuto || isBreakBetweenControllable(breakValue))
        return breakValue;
    return BreakAuto;
}

EBreak LayoutBox::breakBefore() const
{
    EBreak breakValue = style()->breakBefore();
    if (breakValue == BreakAuto || isBreakBetweenControllable(breakValue))
        return breakValue;
    return BreakAuto;
}

EBreak LayoutBox::breakInside() const
{
    EBreak breakValue = style()->breakInside();
    if (breakValue == BreakAuto || isBreakInsideControllable(breakValue))
        return breakValue;
    return BreakAuto;
}

// At a class A break point [1], the break value with the highest precedence wins. If the two values
// have the same precedence (e.g. "left" and "right"), the value specified on a latter object wins.
//
// [1] https://drafts.csswg.org/css-break/#possible-breaks
static inline int fragmentainerBreakPrecedence(EBreak breakValue)
{
    // "auto" has the lowest priority.
    // "avoid*" values win over "auto".
    // "avoid-page" wins over "avoid-column".
    // "avoid" wins over "avoid-page".
    // Forced break values win over "avoid".
    // Any forced page break value wins over "column" forced break.
    // More specific break values (left, right, recto, verso) wins over generic "page" values.

    switch (breakValue) {
    default:
        ASSERT_NOT_REACHED();
        // fall-through
    case BreakAuto:
        return 0;
    case BreakAvoidColumn:
        return 1;
    case BreakAvoidPage:
        return 2;
    case BreakAvoid:
        return 3;
    case BreakColumn:
        return 4;
    case BreakPage:
        return 5;
    case BreakLeft:
    case BreakRight:
    case BreakRecto:
    case BreakVerso:
        return 6;
    }
}

EBreak LayoutBox::joinFragmentainerBreakValues(EBreak firstValue, EBreak secondValue)
{
    if (fragmentainerBreakPrecedence(secondValue) >= fragmentainerBreakPrecedence(firstValue))
        return secondValue;
    return firstValue;
}

EBreak LayoutBox::classABreakPointValue(EBreak previousBreakAfterValue) const
{
    ASSERT(isBreakBetweenControllable(previousBreakAfterValue));
    return joinFragmentainerBreakValues(previousBreakAfterValue, breakBefore());
}

bool LayoutBox::needsForcedBreakBefore(EBreak previousBreakAfterValue) const
{
    return isForcedFragmentainerBreakValue(classABreakPointValue(previousBreakAfterValue));
}

LayoutRect LayoutBox::localOverflowRectForPaintInvalidation() const
{
    if (style()->visibility() != VISIBLE)
        return LayoutRect();

    return selfVisualOverflowRect();
}

void LayoutBox::inflateVisualRectForReflectionAndFilterUnderContainer(LayoutRect& rect, const LayoutObject& container, const LayoutBoxModelObject* ancestorToStopAt) const
{
    // Apply visual overflow caused by reflections and filters defined on objects between this object
    // and container (not included) or ancestorToStopAt (included).
    LayoutSize offsetFromContainer = this->offsetFromContainer(&container);
    rect.move(offsetFromContainer);
    for (LayoutObject* parent = this->parent(); parent && parent != container; parent = parent->parent()) {
        if (parent->isBox()) {
            // Convert rect into coordinate space of parent to apply parent's reflection and filter.
            LayoutSize parentOffset = parent->offsetFromAncestorContainer(&container);
            rect.move(-parentOffset);
            toLayoutBox(parent)->inflateVisualRectForReflectionAndFilter(rect);
            rect.move(parentOffset);
        }
        if (parent == ancestorToStopAt)
            break;
    }
    rect.move(-offsetFromContainer);
}

bool LayoutBox::mapToVisualRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect& rect, VisualRectFlags visualRectFlags) const
{
    inflateVisualRectForReflectionAndFilter(rect);

    if (ancestor == this) {
        // The final rect returned is always in the physical coordinate space of the ancestor.
        flipForWritingMode(rect);
        return true;
    }

    bool ancestorSkipped;
    bool filterOrReflectionSkipped;
    LayoutObject* container = this->container(ancestor, &ancestorSkipped, &filterOrReflectionSkipped);
    if (!container)
        return true;

    if (filterOrReflectionSkipped)
        inflateVisualRectForReflectionAndFilterUnderContainer(rect, *container, ancestor);

    // The rect we compute at each step is shifted by our x/y offset in the parent container's coordinate space.
    // Only when we cross a writing mode boundary will we have to possibly flipForWritingMode (to convert into a more
    // appropriate offset corner for the enclosing container).
    if (isWritingModeRoot()) {
        flipForWritingMode(rect);
        // Then flip rect currently in physical direction to container's block direction.
        if (container->styleRef().isFlippedBlocksWritingMode())
            rect.setX(m_frameRect.width() - rect.maxX());
    }

    LayoutPoint topLeft = rect.location();
    topLeft.move(locationOffset());

    // We are now in our parent container's coordinate space.  Apply our transform to obtain a bounding box
    // in the parent's coordinate space that encloses us.
    if (hasLayer() && layer()->transform()) {
        // Use enclosingIntRect because we cannot properly compute pixel snapping for painted elements within
        // the transform since we don't know the desired subpixel accumulation at this point, and the transform may
        // include a scale.
        rect = LayoutRect(layer()->transform()->mapRect(enclosingIntRect(rect)));
        topLeft = rect.location();
        topLeft.move(locationOffset());
    }

    const ComputedStyle& styleToUse = styleRef();
    EPosition position = styleToUse.position();
    if (position == AbsolutePosition && container->isInFlowPositioned() && container->isLayoutInline()) {
        topLeft += toLayoutInline(container)->offsetForInFlowPositionedInline(*this);
    } else if (styleToUse.hasInFlowPosition() && layer()) {
        // Apply the relative position offset when invalidating a rectangle.  The layer
        // is translated, but the layout box isn't, so we need to do this to get the
        // right dirty rect.  Since this is called from LayoutObject::setStyle, the relative position
        // flag on the LayoutObject has been cleared, so use the one on the style().
        topLeft += layer()->offsetForInFlowPosition();
    }

    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    rect.setLocation(topLeft);

    if (container->isBox() && !toLayoutBox(container)->mapScrollingContentsRectToBoxSpace(rect, container == ancestor ? ApplyNonScrollOverflowClip : ApplyOverflowClip, visualRectFlags))
        return false;

    if (ancestorSkipped) {
        // If the ancestor is below o, then we need to map the rect into ancestor's coordinates.
        LayoutSize containerOffset = ancestor->offsetFromAncestorContainer(container);
        rect.move(-containerOffset);
        // If the ancestor is fixed, then the rect is already in its coordinates so doesn't need viewport-adjusting.
        if (ancestor->style()->position() != FixedPosition && container->isLayoutView() && position == FixedPosition)
            toLayoutView(container)->adjustOffsetForFixedPosition(rect);
        return true;
    }

    if (container->isLayoutView())
        return toLayoutView(container)->mapToVisualRectInAncestorSpace(ancestor, rect, position == FixedPosition ? IsFixed : 0, visualRectFlags);
    else
        return container->mapToVisualRectInAncestorSpace(ancestor, rect, visualRectFlags);
}

void LayoutBox::inflateVisualRectForReflectionAndFilter(LayoutRect& paintInvalidationRect) const
{
    if (!RuntimeEnabledFeatures::cssBoxReflectFilterEnabled() && hasReflection())
        paintInvalidationRect.unite(reflectedRect(paintInvalidationRect));

    if (layer() && layer()->hasFilterInducingProperty())
        paintInvalidationRect = layer()->mapLayoutRectForFilter(paintInvalidationRect);
}

void LayoutBox::invalidatePaintForOverhangingFloats(bool)
{
}

void LayoutBox::updateLogicalWidth()
{
    LogicalExtentComputedValues computedValues;
    computeLogicalWidth(computedValues);

    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);
}

static float getMaxWidthListMarker(const LayoutBox* layoutObject)
{
#if ENABLE(ASSERT)
    ASSERT(layoutObject);
    Node* parentNode = layoutObject->generatingNode();
    ASSERT(parentNode);
    ASSERT(isHTMLOListElement(parentNode) || isHTMLUListElement(parentNode));
    ASSERT(layoutObject->style()->textAutosizingMultiplier() != 1);
#endif
    float maxWidth = 0;
    for (LayoutObject* child = layoutObject->slowFirstChild(); child; child = child->nextSibling()) {
        if (!child->isListItem())
            continue;

        LayoutBox* listItem = toLayoutBox(child);
        for (LayoutObject* itemChild = listItem->slowFirstChild(); itemChild; itemChild = itemChild->nextSibling()) {
            if (!itemChild->isListMarker())
                continue;
            LayoutBox* itemMarker = toLayoutBox(itemChild);
            // Make sure to compute the autosized width.
            if (itemMarker->needsLayout())
                itemMarker->layout();
            maxWidth = std::max<float>(maxWidth, toLayoutListMarker(itemMarker)->logicalWidth().toFloat());
            break;
        }
    }
    return maxWidth;
}

void LayoutBox::computeLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = style()->containsSize() ? borderAndPaddingLogicalWidth() : logicalWidth();
    computedValues.m_position = logicalLeft();
    computedValues.m_margins.m_start = marginStart();
    computedValues.m_margins.m_end = marginEnd();

    if (isOutOfFlowPositioned()) {
        computePositionedLogicalWidth(computedValues);
        return;
    }

    // The parent box is flexing us, so it has increased or decreased our
    // width.  Use the width from the style context.
    // FIXME: Account for writing-mode in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    if (hasOverrideLogicalContentWidth() && parent()->isFlexibleBoxIncludingDeprecated()) {
        computedValues.m_extent = overrideLogicalContentWidth() + borderAndPaddingLogicalWidth();
        return;
    }

    // FIXME: Account for writing-mode in flexible boxes.
    // https://bugs.webkit.org/show_bug.cgi?id=46418
    bool inVerticalBox = parent()->isDeprecatedFlexibleBox() && (parent()->style()->boxOrient() == VERTICAL);
    bool stretching = (parent()->style()->boxAlign() == BSTRETCH);
    // TODO (lajava): Stretching is the only reason why we don't want the box to be treated as a replaced element, so we could perhaps
    // refactor all this logic, not only for flex and grid since alignment is intended to be applied to any block.
    bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inVerticalBox || !stretching) && (!isGridItem() || !hasStretchedLogicalWidth());
    const ComputedStyle& styleToUse = styleRef();
    Length logicalWidthLength = treatAsReplaced ? Length(computeReplacedLogicalWidth(), Fixed) : styleToUse.logicalWidth();

    LayoutBlock* cb = containingBlock();
    LayoutUnit containerLogicalWidth = std::max(LayoutUnit(), containingBlockLogicalWidthForContent());
    bool hasPerpendicularContainingBlock = cb->isHorizontalWritingMode() != isHorizontalWritingMode();

    if (isInline() && !isInlineBlockOrInlineTable()) {
        // just calculate margins
        computedValues.m_margins.m_start = minimumValueForLength(styleToUse.marginStart(), containerLogicalWidth);
        computedValues.m_margins.m_end = minimumValueForLength(styleToUse.marginEnd(), containerLogicalWidth);
        if (treatAsReplaced)
            computedValues.m_extent = std::max(LayoutUnit(floatValueForLength(logicalWidthLength, 0)) + borderAndPaddingLogicalWidth(), minPreferredLogicalWidth());
        return;
    }

    LayoutUnit containerWidthInInlineDirection = containerLogicalWidth;
    if (hasPerpendicularContainingBlock)
        containerWidthInInlineDirection = perpendicularContainingBlockLogicalHeight();

    // Width calculations
    if (treatAsReplaced) {
        computedValues.m_extent = LayoutUnit(logicalWidthLength.value()) + borderAndPaddingLogicalWidth();
    } else if (parent()->isLayoutGrid() && style()->logicalWidth().isAuto() && style()->logicalMinWidth().isAuto() && style()->overflowX() == OverflowVisible && containerWidthInInlineDirection < minPreferredLogicalWidth()) {
        // TODO (lajava) Move this logic to the LayoutGrid class.
        // Implied minimum size of Grid items.
        computedValues.m_extent = constrainLogicalWidthByMinMax(minPreferredLogicalWidth(), containerWidthInInlineDirection, cb);
    } else {
        LayoutUnit preferredWidth = computeLogicalWidthUsing(MainOrPreferredSize, styleToUse.logicalWidth(), containerWidthInInlineDirection, cb);
        computedValues.m_extent = constrainLogicalWidthByMinMax(preferredWidth, containerWidthInInlineDirection, cb);
    }

    // Margin calculations.
    computeMarginsForDirection(InlineDirection, cb, containerLogicalWidth, computedValues.m_extent, computedValues.m_margins.m_start,
        computedValues.m_margins.m_end, style()->marginStart(), style()->marginEnd());

    if (!hasPerpendicularContainingBlock && containerLogicalWidth && containerLogicalWidth != (computedValues.m_extent + computedValues.m_margins.m_start + computedValues.m_margins.m_end)
        && !isFloating() && !isInline() && !cb->isFlexibleBoxIncludingDeprecated() && !cb->isLayoutGrid()) {
        LayoutUnit newMargin = containerLogicalWidth - computedValues.m_extent - cb->marginStartForChild(*this);
        bool hasInvertedDirection = cb->style()->isLeftToRightDirection() != style()->isLeftToRightDirection();
        if (hasInvertedDirection)
            computedValues.m_margins.m_start = newMargin;
        else
            computedValues.m_margins.m_end = newMargin;
    }

    if (styleToUse.textAutosizingMultiplier() != 1 && styleToUse.marginStart().type() == Fixed) {
        Node* parentNode = generatingNode();
        if (parentNode && (isHTMLOListElement(*parentNode) || isHTMLUListElement(*parentNode))) {
            // Make sure the markers in a list are properly positioned (i.e. not chopped off) when autosized.
            const float adjustedMargin = (1 - 1.0 / styleToUse.textAutosizingMultiplier()) * getMaxWidthListMarker(this);
            bool hasInvertedDirection = cb->style()->isLeftToRightDirection() != style()->isLeftToRightDirection();
            if (hasInvertedDirection)
                computedValues.m_margins.m_end += adjustedMargin;
            else
                computedValues.m_margins.m_start += adjustedMargin;
        }
    }
}

LayoutUnit LayoutBox::fillAvailableMeasure(LayoutUnit availableLogicalWidth) const
{
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    return fillAvailableMeasure(availableLogicalWidth, marginStart, marginEnd);
}

LayoutUnit LayoutBox::fillAvailableMeasure(LayoutUnit availableLogicalWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd) const
{
    ASSERT(availableLogicalWidth >= 0);
    marginStart = minimumValueForLength(style()->marginStart(), availableLogicalWidth);
    marginEnd = minimumValueForLength(style()->marginEnd(), availableLogicalWidth);
    LayoutUnit available = availableLogicalWidth - marginStart - marginEnd;
    available = std::max(available, LayoutUnit());
    return available;
}

LayoutUnit LayoutBox::computeIntrinsicLogicalWidthUsing(const Length& logicalWidthLength, LayoutUnit availableLogicalWidth, LayoutUnit borderAndPadding) const
{
    if (logicalWidthLength.type() == FillAvailable)
        return std::max(borderAndPadding, fillAvailableMeasure(availableLogicalWidth));

    LayoutUnit minLogicalWidth;
    LayoutUnit maxLogicalWidth;
    computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);

    if (logicalWidthLength.type() == MinContent)
        return minLogicalWidth + borderAndPadding;

    if (logicalWidthLength.type() == MaxContent)
        return maxLogicalWidth + borderAndPadding;

    if (logicalWidthLength.type() == FitContent) {
        minLogicalWidth += borderAndPadding;
        maxLogicalWidth += borderAndPadding;
        return std::max(minLogicalWidth, std::min(maxLogicalWidth, fillAvailableMeasure(availableLogicalWidth)));
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

LayoutUnit LayoutBox::computeLogicalWidthUsing(SizeType widthType, const Length& logicalWidth, LayoutUnit availableLogicalWidth, const LayoutBlock* cb) const
{
    ASSERT(widthType == MinSize || widthType == MainOrPreferredSize || !logicalWidth.isAuto());
    if (widthType == MinSize && logicalWidth.isAuto())
        return adjustBorderBoxLogicalWidthForBoxSizing(0);

    if (!logicalWidth.isIntrinsicOrAuto()) {
        // FIXME: If the containing block flow is perpendicular to our direction we need to use the available logical height instead.
        return adjustBorderBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, availableLogicalWidth));
    }

    if (logicalWidth.isIntrinsic())
        return computeIntrinsicLogicalWidthUsing(logicalWidth, availableLogicalWidth, borderAndPaddingLogicalWidth());

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    LayoutUnit logicalWidthResult = fillAvailableMeasure(availableLogicalWidth, marginStart, marginEnd);

    if (shrinkToAvoidFloats() && cb->isLayoutBlockFlow() && toLayoutBlockFlow(cb)->containsFloats())
        logicalWidthResult = std::min(logicalWidthResult, shrinkLogicalWidthToAvoidFloats(marginStart, marginEnd, toLayoutBlockFlow(cb)));

    if (widthType == MainOrPreferredSize && sizesLogicalWidthToFitContent(logicalWidth))
        return std::max(minPreferredLogicalWidth(), std::min(maxPreferredLogicalWidth(), logicalWidthResult));
    return logicalWidthResult;
}

static bool columnFlexItemHasStretchAlignment(const LayoutObject* flexitem)
{
    LayoutObject* parent = flexitem->parent();
    // auto margins mean we don't stretch. Note that this function will only be used for
    // widths, so we don't have to check marginBefore/marginAfter.
    ASSERT(parent->style()->isColumnFlexDirection());
    if (flexitem->style()->marginStart().isAuto() || flexitem->style()->marginEnd().isAuto())
        return false;
    return flexitem->style()->alignSelfPosition() == ItemPositionStretch || (flexitem->style()->alignSelfPosition() == ItemPositionAuto && parent->style()->alignItemsPosition() == ItemPositionStretch);
}

static bool isStretchingColumnFlexItem(const LayoutObject* flexitem)
{
    LayoutObject* parent = flexitem->parent();
    if (parent->isDeprecatedFlexibleBox() && parent->style()->boxOrient() == VERTICAL && parent->style()->boxAlign() == BSTRETCH)
        return true;

    // We don't stretch multiline flexboxes because they need to apply line spacing (align-content) first.
    if (parent->isFlexibleBox() && parent->style()->flexWrap() == FlexNoWrap && parent->style()->isColumnFlexDirection() && columnFlexItemHasStretchAlignment(flexitem))
        return true;
    return false;
}

// TODO (lajava) Can/Should we move this inside specific layout classes (flex. grid)? Can we refactor columnFlexItemHasStretchAlignment logic?
bool LayoutBox::hasStretchedLogicalWidth() const
{
    const ComputedStyle& style = styleRef();
    if (!style.logicalWidth().isAuto() || style.marginStart().isAuto() || style.marginEnd().isAuto())
        return false;
    LayoutBlock* cb = containingBlock();
    if (!cb) {
        // We are evaluating align-self/justify-self, which default to 'normal' for the root element.
        // The 'normal' value behaves like 'start' except for Flexbox Items, which obviously should have a container.
        return false;
    }
    if (cb->isHorizontalWritingMode() != isHorizontalWritingMode())
        return ComputedStyle::resolveAlignment(cb->styleRef(), style, ItemPositionStretch) == ItemPositionStretch;
    return ComputedStyle::resolveJustification(cb->styleRef(), style, ItemPositionStretch) == ItemPositionStretch;
}

bool LayoutBox::sizesLogicalWidthToFitContent(const Length& logicalWidth) const
{
    if (isFloating() || isInlineBlockOrInlineTable())
        return true;

    if (isGridItem())
        return !hasStretchedLogicalWidth();

    // Flexible box items should shrink wrap, so we lay them out at their intrinsic widths.
    // In the case of columns that have a stretch alignment, we go ahead and layout at the
    // stretched size to avoid an extra layout when applying alignment.
    if (parent()->isFlexibleBox()) {
        // For multiline columns, we need to apply align-content first, so we can't stretch now.
        if (!parent()->style()->isColumnFlexDirection() || parent()->style()->flexWrap() != FlexNoWrap)
            return true;
        if (!columnFlexItemHasStretchAlignment(this))
            return true;
    }

    // Flexible horizontal boxes lay out children at their intrinsic widths.  Also vertical boxes
    // that don't stretch their kids lay out their children at their intrinsic widths.
    // FIXME: Think about writing-mode here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (parent()->isDeprecatedFlexibleBox() && (parent()->style()->boxOrient() == HORIZONTAL || parent()->style()->boxAlign() != BSTRETCH))
        return true;

    // Button, input, select, textarea, and legend treat width value of 'auto' as 'intrinsic' unless it's in a
    // stretching column flexbox.
    // FIXME: Think about writing-mode here.
    // https://bugs.webkit.org/show_bug.cgi?id=46473
    if (logicalWidth.isAuto() && !isStretchingColumnFlexItem(this) && autoWidthShouldFitContent())
        return true;

    if (isHorizontalWritingMode() != containingBlock()->isHorizontalWritingMode())
        return true;

    return false;
}

bool LayoutBox::autoWidthShouldFitContent() const
{
    return node() && (isHTMLInputElement(*node()) || isHTMLSelectElement(*node()) || isHTMLButtonElement(*node())
        || isHTMLTextAreaElement(*node()) || (isHTMLLegendElement(*node()) && !style()->hasOutOfFlowPosition()));
}

void LayoutBox::computeMarginsForDirection(MarginDirection flowDirection, const LayoutBlock* containingBlock, LayoutUnit containerWidth, LayoutUnit childWidth, LayoutUnit& marginStart, LayoutUnit& marginEnd, Length marginStartLength, Length marginEndLength) const
{
    // First assert that we're not calling this method on box types that don't support margins.
    ASSERT(!isTableCell());
    ASSERT(!isTableRow());
    ASSERT(!isTableSection());
    ASSERT(!isLayoutTableCol());
    if (flowDirection == BlockDirection || isFloating() || isInline()) {
        // Margins are calculated with respect to the logical width of
        // the containing block (8.3)
        // Inline blocks/tables and floats don't have their margins increased.
        marginStart = minimumValueForLength(marginStartLength, containerWidth);
        marginEnd = minimumValueForLength(marginEndLength, containerWidth);
        return;
    }

    if (containingBlock->isFlexibleBox()) {
        // We need to let flexbox handle the margin adjustment - otherwise, flexbox
        // will think we're wider than we actually are and calculate line sizes wrong.
        // See also http://dev.w3.org/csswg/css-flexbox/#auto-margins
        if (marginStartLength.isAuto())
            marginStartLength.setValue(0);
        if (marginEndLength.isAuto())
            marginEndLength.setValue(0);
    }

    LayoutUnit marginStartWidth = minimumValueForLength(marginStartLength, containerWidth);
    LayoutUnit marginEndWidth = minimumValueForLength(marginEndLength, containerWidth);

    LayoutUnit availableWidth = containerWidth;
    if (avoidsFloats() && containingBlock->isLayoutBlockFlow() && toLayoutBlockFlow(containingBlock)->containsFloats()) {
        availableWidth = containingBlockAvailableLineWidth();
        if (shrinkToAvoidFloats() && availableWidth < containerWidth) {
            marginStart = std::max(LayoutUnit(), marginStartWidth);
            marginEnd = std::max(LayoutUnit(), marginEndWidth);
        }
    }

    // CSS 2.1 (10.3.3): "If 'width' is not 'auto' and 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width'
    // (plus any of 'margin-left' or 'margin-right' that are not 'auto') is larger than the width of the containing block, then any 'auto'
    // values for 'margin-left' or 'margin-right' are, for the following rules, treated as zero.
    LayoutUnit marginBoxWidth = childWidth + (!style()->width().isAuto() ? marginStartWidth + marginEndWidth : LayoutUnit());

    if (marginBoxWidth < availableWidth) {
        // CSS 2.1: "If both 'margin-left' and 'margin-right' are 'auto', their used values are equal. This horizontally centers the element
        // with respect to the edges of the containing block."
        const ComputedStyle& containingBlockStyle = containingBlock->styleRef();
        if ((marginStartLength.isAuto() && marginEndLength.isAuto())
            || (!marginStartLength.isAuto() && !marginEndLength.isAuto() && containingBlockStyle.textAlign() == WEBKIT_CENTER)) {
            // Other browsers center the margin box for align=center elements so we match them here.
            LayoutUnit centeredMarginBoxStart = std::max(LayoutUnit(), (availableWidth - childWidth - marginStartWidth - marginEndWidth) / 2);
            marginStart = centeredMarginBoxStart + marginStartWidth;
            marginEnd = availableWidth - childWidth - marginStart + marginEndWidth;
            return;
        }

        // Adjust margins for the align attribute
        if ((!containingBlockStyle.isLeftToRightDirection() && containingBlockStyle.textAlign() == WEBKIT_LEFT)
            || (containingBlockStyle.isLeftToRightDirection() && containingBlockStyle.textAlign() == WEBKIT_RIGHT)) {
            if (containingBlockStyle.isLeftToRightDirection() != styleRef().isLeftToRightDirection()) {
                if (!marginStartLength.isAuto())
                    marginEndLength = Length(Auto);
            } else {
                if (!marginEndLength.isAuto())
                    marginStartLength = Length(Auto);
            }
        }

        // CSS 2.1: "If there is exactly one value specified as 'auto', its used value follows from the equality."
        if (marginEndLength.isAuto()) {
            marginStart = marginStartWidth;
            marginEnd = availableWidth - childWidth - marginStart;
            return;
        }

        if (marginStartLength.isAuto()) {
            marginEnd = marginEndWidth;
            marginStart = availableWidth - childWidth - marginEnd;
            return;
        }
    }

    // Either no auto margins, or our margin box width is >= the container width, auto margins will just turn into 0.
    marginStart = marginStartWidth;
    marginEnd = marginEndWidth;
}

void LayoutBox::updateLogicalHeight()
{
    m_intrinsicContentLogicalHeight = contentLogicalHeight();

    LogicalExtentComputedValues computedValues;
    LayoutUnit height = style()->containsSize() ? borderAndPaddingLogicalHeight() : logicalHeight();
    computeLogicalHeight(height, logicalTop(), computedValues);

    setLogicalHeight(computedValues.m_extent);
    setLogicalTop(computedValues.m_position);
    setMarginBefore(computedValues.m_margins.m_before);
    setMarginAfter(computedValues.m_margins.m_after);
}

void LayoutBox::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;

    // Cell height is managed by the table.
    if (isTableCell())
        return;

    Length h;
    if (isOutOfFlowPositioned()) {
        computePositionedLogicalHeight(computedValues);
    } else {
        LayoutBlock* cb = containingBlock();

        // If we are perpendicular to our containing block then we need to resolve our block-start and block-end margins so that if they
        // are 'auto' we are centred or aligned within the inline flow containing block: this is done by computing the margins as though they are inline.
        // Note that as this is the 'sizing phase' we are using our own writing mode rather than the containing block's. We use the containing block's
        // writing mode when figuring out the block-direction margins for positioning in |computeAndSetBlockDirectionMargins| (i.e. margin collapsing etc.).
        // See http://www.w3.org/TR/2014/CR-css-writing-modes-3-20140320/#orthogonal-flows
        MarginDirection flowDirection = isHorizontalWritingMode() != cb->isHorizontalWritingMode() ? InlineDirection : BlockDirection;

        // For tables, calculate margins only.
        if (isTable()) {
            computeMarginsForDirection(flowDirection, cb, containingBlockLogicalWidthForContent(), computedValues.m_extent, computedValues.m_margins.m_before,
                computedValues.m_margins.m_after, style()->marginBefore(), style()->marginAfter());
            return;
        }

        // FIXME: Account for writing-mode in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        bool inHorizontalBox = parent()->isDeprecatedFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = shouldComputeSizeAsReplaced() && (!inHorizontalBox || !stretching);
        bool checkMinMaxHeight = false;

        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        // FIXME: Account for writing-mode in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (hasOverrideLogicalContentHeight()) {
            LayoutUnit contentHeight = overrideLogicalContentHeight();
            if (parent()->isLayoutGrid() && style()->logicalMinHeight().isAuto() && style()->overflowY() == OverflowVisible) {
                ASSERT(style()->logicalHeight().isAuto());
                LayoutUnit minContentHeight = computeContentLogicalHeight(MinSize, Length(MinContent), computedValues.m_extent - borderAndPaddingLogicalHeight());
                contentHeight = std::max(contentHeight, constrainContentBoxLogicalHeightByMinMax(minContentHeight, computedValues.m_extent - borderAndPaddingLogicalHeight()));
            }
            h = Length(contentHeight, Fixed);
        } else if (treatAsReplaced) {
            h = Length(computeReplacedLogicalHeight(), Fixed);
        } else {
            h = style()->logicalHeight();
            checkMinMaxHeight = true;
        }

        // Block children of horizontal flexible boxes fill the height of the box.
        // FIXME: Account for writing-mode in flexible boxes.
        // https://bugs.webkit.org/show_bug.cgi?id=46418
        if (h.isAuto() && inHorizontalBox && toLayoutDeprecatedFlexibleBox(parent())->isStretchingChildren()) {
            h = Length(parentBox()->contentLogicalHeight() - marginBefore() - marginAfter() - borderAndPaddingLogicalHeight(), Fixed);
            checkMinMaxHeight = false;
        }

        LayoutUnit heightResult;
        if (checkMinMaxHeight) {
            heightResult = computeLogicalHeightUsing(MainOrPreferredSize, style()->logicalHeight(), computedValues.m_extent - borderAndPaddingLogicalHeight());
            if (heightResult == -1)
                heightResult = computedValues.m_extent;
            heightResult = constrainLogicalHeightByMinMax(heightResult, computedValues.m_extent - borderAndPaddingLogicalHeight());
        } else {
            // The only times we don't check min/max height are when a fixed length has
            // been given as an override.  Just use that.  The value has already been adjusted
            // for box-sizing.
            ASSERT(h.isFixed());
            heightResult = LayoutUnit(h.value()) + borderAndPaddingLogicalHeight();
        }

        computedValues.m_extent = heightResult;
        computeMarginsForDirection(flowDirection, cb, containingBlockLogicalWidthForContent(), computedValues.m_extent, computedValues.m_margins.m_before,
            computedValues.m_margins.m_after, style()->marginBefore(), style()->marginAfter());
    }

    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode.  The <body> always fills the
    // <html> block in quirks mode.  Only apply this quirk if the block is normal flow and no height
    // is specified. When we're printing, we also need this quirk if the body or root has a percentage
    // height since we don't set a height in LayoutView when we're printing. So without this quirk, the
    // height has nothing to be a percentage of, and it ends up being 0. That is bad.
    bool paginatedContentNeedsBaseHeight = document().printing() && h.hasPercent()
        && (isDocumentElement() || (isBody() && document().documentElement()->layoutObject()->style()->logicalHeight().hasPercent())) && !isInline();
    if (stretchesToViewport() || paginatedContentNeedsBaseHeight) {
        LayoutUnit margins = collapsedMarginBefore() + collapsedMarginAfter();
        LayoutUnit visibleHeight = view()->viewLogicalHeightForPercentages();
        if (isDocumentElement()) {
            computedValues.m_extent = std::max(computedValues.m_extent, visibleHeight - margins);
        } else {
            LayoutUnit marginsBordersPadding = margins + parentBox()->marginBefore() + parentBox()->marginAfter() + parentBox()->borderAndPaddingLogicalHeight();
            computedValues.m_extent = std::max(computedValues.m_extent, visibleHeight - marginsBordersPadding);
        }
    }
}

LayoutUnit LayoutBox::computeLogicalHeightWithoutLayout() const
{
    // TODO(cbiesinger): We should probably return something other than just border + padding, but for now
    // we have no good way to do anything else without layout, so we just use that.
    LogicalExtentComputedValues computedValues;
    computeLogicalHeight(borderAndPaddingLogicalHeight(), LayoutUnit(), computedValues);
    return computedValues.m_extent;
}

LayoutUnit LayoutBox::computeLogicalHeightUsing(SizeType heightType, const Length& height, LayoutUnit intrinsicContentHeight) const
{
    LayoutUnit logicalHeight = computeContentAndScrollbarLogicalHeightUsing(heightType, height, intrinsicContentHeight);
    if (logicalHeight != -1) {
        if (height.isSpecified())
            logicalHeight = adjustBorderBoxLogicalHeightForBoxSizing(logicalHeight);
        else
            logicalHeight += borderAndPaddingLogicalHeight();
    }
    return logicalHeight;
}

LayoutUnit LayoutBox::computeContentLogicalHeight(SizeType heightType, const Length& height, LayoutUnit intrinsicContentHeight) const
{
    LayoutUnit heightIncludingScrollbar = computeContentAndScrollbarLogicalHeightUsing(heightType, height, intrinsicContentHeight);
    if (heightIncludingScrollbar == -1)
        return LayoutUnit(-1);
    LayoutUnit adjusted = heightIncludingScrollbar;
    if (height.isSpecified()) {
        // Keywords don't get adjusted for box-sizing
        adjusted = adjustContentBoxLogicalHeightForBoxSizing(heightIncludingScrollbar);
    }
    return std::max(LayoutUnit(), adjusted - scrollbarLogicalHeight());
}

LayoutUnit LayoutBox::computeIntrinsicLogicalContentHeightUsing(const Length& logicalHeightLength, LayoutUnit intrinsicContentHeight, LayoutUnit borderAndPadding) const
{
    // FIXME(cbiesinger): The css-sizing spec is considering changing what min-content/max-content should resolve to.
    // If that happens, this code will have to change.
    if (logicalHeightLength.isMinContent() || logicalHeightLength.isMaxContent() || logicalHeightLength.isFitContent()) {
        if (isAtomicInlineLevel())
            return intrinsicSize().height();
        return intrinsicContentHeight;
    }
    if (logicalHeightLength.isFillAvailable())
        return containingBlock()->availableLogicalHeight(ExcludeMarginBorderPadding) - borderAndPadding;
    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

LayoutUnit LayoutBox::computeContentAndScrollbarLogicalHeightUsing(SizeType heightType, const Length& height, LayoutUnit intrinsicContentHeight) const
{
    if (height.isAuto())
        return heightType == MinSize ? LayoutUnit() : LayoutUnit(-1);
    // FIXME(cbiesinger): The css-sizing spec is considering changing what min-content/max-content should resolve to.
    // If that happens, this code will have to change.
    if (height.isIntrinsic()) {
        if (intrinsicContentHeight == -1)
            return LayoutUnit(-1); // Intrinsic height isn't available.
        return computeIntrinsicLogicalContentHeightUsing(height, intrinsicContentHeight, borderAndPaddingLogicalHeight()) + scrollbarLogicalHeight();
    }
    if (height.isFixed())
        return LayoutUnit(height.value());
    if (height.hasPercent())
        return computePercentageLogicalHeight(height);
    return LayoutUnit(-1);
}

bool LayoutBox::stretchesToViewportInQuirksMode() const
{
    if (!isDocumentElement() && !isBody())
        return false;
    return style()->logicalHeight().isAuto() && !isFloatingOrOutOfFlowPositioned() && !isInline() && !flowThreadContainingBlock();
}

bool LayoutBox::skipContainingBlockForPercentHeightCalculation(const LayoutBox* containingBlock) const
{
    // If the writing mode of the containing block is orthogonal to ours, it means that we shouldn't
    // skip anything, since we're going to resolve the percentage height against a containing block *width*.
    if (isHorizontalWritingMode() != containingBlock->isHorizontalWritingMode())
        return false;

    // Anonymous blocks should not impede percentage resolution on a child. Examples of such
    // anonymous blocks are blocks wrapped around inlines that have block siblings (from the CSS
    // spec) and multicol flow threads (an implementation detail). Another implementation detail,
    // ruby runs, create anonymous inline-blocks, so skip those too. All other types of anonymous
    // objects, such as table-cells, will be treated just as if they were non-anonymous.
    if (containingBlock->isAnonymous()) {
        EDisplay display = containingBlock->styleRef().display();
        return display == BLOCK || display == INLINE_BLOCK;
    }

    // For quirks mode, we skip most auto-height containing blocks when computing percentages.
    return document().inQuirksMode() && !containingBlock->isTableCell() && !containingBlock->isOutOfFlowPositioned() && !containingBlock->isLayoutGrid() && containingBlock->style()->logicalHeight().isAuto();
}

LayoutUnit LayoutBox::computePercentageLogicalHeight(const Length& height) const
{
    LayoutUnit availableHeight(-1);

    bool skippedAutoHeightContainingBlock = false;
    LayoutBlock* cb = containingBlock();
    const LayoutBox* containingBlockChild = this;
    LayoutUnit rootMarginBorderPaddingHeight;
    while (!cb->isLayoutView() && skipContainingBlockForPercentHeightCalculation(cb)) {
        if (cb->isBody() || cb->isDocumentElement())
            rootMarginBorderPaddingHeight += cb->marginBefore() + cb->marginAfter() + cb->borderAndPaddingLogicalHeight();
        skippedAutoHeightContainingBlock = true;
        containingBlockChild = cb;
        cb = cb->containingBlock();
    }
    cb->addPercentHeightDescendant(const_cast<LayoutBox*>(this));

    const ComputedStyle& cbstyle = cb->styleRef();

    // A positioned element that specified both top/bottom or that specifies height should be treated as though it has a height
    // explicitly specified that can be used for any percentage computations.
    bool isOutOfFlowPositionedWithSpecifiedHeight = cb->isOutOfFlowPositioned() && (!cbstyle.logicalHeight().isAuto() || (!cbstyle.logicalTop().isAuto() && !cbstyle.logicalBottom().isAuto()));

    bool includeBorderPadding = isTable();

    LayoutUnit stretchedFlexHeight(-1);
    if (cb->isFlexItem())
        stretchedFlexHeight = toLayoutFlexibleBox(cb->parent())->childLogicalHeightForPercentageResolution(*cb);

    if (isHorizontalWritingMode() != cb->isHorizontalWritingMode()) {
        availableHeight = containingBlockChild->containingBlockLogicalWidthForContent();
    } else if (stretchedFlexHeight != LayoutUnit(-1)) {
        availableHeight = stretchedFlexHeight;
    } else if (hasOverrideContainingBlockLogicalHeight() && !isOutOfFlowPositionedWithSpecifiedHeight) {
        availableHeight = overrideContainingBlockContentLogicalHeight();
    } else if (cb->isGridItem() && cb->hasOverrideLogicalContentHeight()) {
        availableHeight = cb->overrideLogicalContentHeight();
    } else if (cb->isTableCell()) {
        if (!skippedAutoHeightContainingBlock) {
            // Table cells violate what the CSS spec says to do with heights. Basically we
            // don't care if the cell specified a height or not. We just always make ourselves
            // be a percentage of the cell's current content height.
            if (!cb->hasOverrideLogicalContentHeight()) {
                // Normally we would let the cell size intrinsically, but scrolling overflow has to be
                // treated differently, since WinIE lets scrolled overflow regions shrink as needed.
                // While we can't get all cases right, we can at least detect when the cell has a specified
                // height or when the table has a specified height. In these cases we want to initially have
                // no size and allow the flexing of the table or the cell to its specified height to cause us
                // to grow to fill the space. This could end up being wrong in some cases, but it is
                // preferable to the alternative (sizing intrinsically and making the row end up too big).
                LayoutTableCell* cell = toLayoutTableCell(cb);
                if (scrollsOverflowY() && (!cell->style()->logicalHeight().isAuto() || !cell->table()->style()->logicalHeight().isAuto()))
                    return LayoutUnit();
                return LayoutUnit(-1);
            }
            availableHeight = cb->overrideLogicalContentHeight();
            includeBorderPadding = true;
        }
    } else if (cbstyle.logicalHeight().isFixed()) {
        LayoutUnit contentBoxHeight = cb->adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit(cbstyle.logicalHeight().value()));
        availableHeight = std::max(LayoutUnit(), cb->constrainContentBoxLogicalHeightByMinMax(contentBoxHeight - cb->scrollbarLogicalHeight(), LayoutUnit(-1)));
    } else if (cbstyle.logicalHeight().hasPercent() && !isOutOfFlowPositionedWithSpecifiedHeight) {
        // We need to recur and compute the percentage height for our containing block.
        LayoutUnit heightWithScrollbar = cb->computePercentageLogicalHeight(cbstyle.logicalHeight());
        if (heightWithScrollbar != -1) {
            LayoutUnit contentBoxHeightWithScrollbar = cb->adjustContentBoxLogicalHeightForBoxSizing(heightWithScrollbar);
            // We need to adjust for min/max height because this method does not
            // handle the min/max of the current block, its caller does. So the
            // return value from the recursive call will not have been adjusted
            // yet.
            LayoutUnit contentBoxHeight = cb->constrainContentBoxLogicalHeightByMinMax(contentBoxHeightWithScrollbar - cb->scrollbarLogicalHeight(), LayoutUnit(-1));
            availableHeight = std::max(LayoutUnit(), contentBoxHeight);
        }
    } else if (isOutOfFlowPositionedWithSpecifiedHeight) {
        // Don't allow this to affect the block' size() member variable, since this
        // can get called while the block is still laying out its kids.
        LogicalExtentComputedValues computedValues;
        cb->computeLogicalHeight(cb->logicalHeight(), LayoutUnit(), computedValues);
        availableHeight = computedValues.m_extent - cb->borderAndPaddingLogicalHeight() - cb->scrollbarLogicalHeight();
    } else if (cb->isLayoutView()) {
        availableHeight = view()->viewLogicalHeightForPercentages();
    }

    if (availableHeight == -1)
        return availableHeight;

    availableHeight -= rootMarginBorderPaddingHeight;

    if (isTable() && isOutOfFlowPositioned())
        availableHeight += cb->paddingLogicalHeight();

    LayoutUnit result = valueForLength(height, availableHeight);
    if (includeBorderPadding) {
        // FIXME: Table cells should default to box-sizing: border-box so we can avoid this hack.
        // It is necessary to use the border-box to match WinIE's broken
        // box model. This is essential for sizing inside
        // table cells using percentage heights.
        result -= borderAndPaddingLogicalHeight();
        return std::max(LayoutUnit(), result);
    }
    return result;
}

LayoutUnit LayoutBox::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(MainOrPreferredSize, style()->logicalWidth()), shouldComputePreferred);
}

LayoutUnit LayoutBox::computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, ShouldComputePreferred shouldComputePreferred) const
{
    LayoutUnit minLogicalWidth = (shouldComputePreferred == ComputePreferred && style()->logicalMinWidth().hasPercent()) ? logicalWidth : computeReplacedLogicalWidthUsing(MinSize, style()->logicalMinWidth());
    LayoutUnit maxLogicalWidth = (shouldComputePreferred == ComputePreferred && style()->logicalMaxWidth().hasPercent()) || style()->logicalMaxWidth().isMaxSizeNone() ? logicalWidth : computeReplacedLogicalWidthUsing(MaxSize, style()->logicalMaxWidth());
    return std::max(minLogicalWidth, std::min(logicalWidth, maxLogicalWidth));
}

LayoutUnit LayoutBox::computeReplacedLogicalWidthUsing(SizeType sizeType, const Length& logicalWidth) const
{
    ASSERT(sizeType == MinSize || sizeType == MainOrPreferredSize || !logicalWidth.isAuto());
    if (sizeType == MinSize && logicalWidth.isAuto())
        return adjustContentBoxLogicalWidthForBoxSizing(LayoutUnit());

    switch (logicalWidth.type()) {
    case Fixed:
        return adjustContentBoxLogicalWidthForBoxSizing(logicalWidth.value());
    case MinContent:
    case MaxContent: {
        // MinContent/MaxContent don't need the availableLogicalWidth argument.
        LayoutUnit availableLogicalWidth;
        return computeIntrinsicLogicalWidthUsing(logicalWidth, availableLogicalWidth, borderAndPaddingLogicalWidth()) - borderAndPaddingLogicalWidth();
    }
    case FitContent:
    case FillAvailable:
    case Percent:
    case Calculated: {
        // FIXME: containingBlockLogicalWidthForContent() is wrong if the replaced element's writing-mode is perpendicular to the
        // containing block's writing-mode.
        // https://bugs.webkit.org/show_bug.cgi?id=46496
        const LayoutUnit cw = isOutOfFlowPositioned() ? containingBlockLogicalWidthForPositioned(toLayoutBoxModelObject(container())) : containingBlockLogicalWidthForContent();
        Length containerLogicalWidth = containingBlock()->style()->logicalWidth();
        // FIXME: Handle cases when containing block width is calculated or viewport percent.
        // https://bugs.webkit.org/show_bug.cgi?id=91071
        if (logicalWidth.isIntrinsic())
            return computeIntrinsicLogicalWidthUsing(logicalWidth, cw, borderAndPaddingLogicalWidth()) - borderAndPaddingLogicalWidth();
        if (cw > 0 || (!cw && (containerLogicalWidth.isFixed() || containerLogicalWidth.hasPercent())))
            return adjustContentBoxLogicalWidthForBoxSizing(minimumValueForLength(logicalWidth, cw));
        return LayoutUnit();
    }
    case Auto:
    case MaxSizeNone:
        return intrinsicLogicalWidth();
    case ExtendToZoom:
    case DeviceWidth:
    case DeviceHeight:
        break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

LayoutUnit LayoutBox::computeReplacedLogicalHeight(LayoutUnit) const
{
    return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(MainOrPreferredSize, style()->logicalHeight()));
}

bool LayoutBox::logicalHeightComputesAsNone(SizeType sizeType) const
{
    ASSERT(sizeType == MinSize || sizeType == MaxSize);
    Length logicalHeight = sizeType == MinSize ? style()->logicalMinHeight() : style()->logicalMaxHeight();
    Length initialLogicalHeight = sizeType == MinSize ? ComputedStyle::initialMinSize() : ComputedStyle::initialMaxSize();

    if (logicalHeight == initialLogicalHeight)
        return true;

    if (LayoutBlock* cb = containingBlockForAutoHeightDetection(logicalHeight))
        return cb->hasAutoHeightOrContainingBlockWithAutoHeight();
    return false;
}

LayoutUnit LayoutBox::computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const
{
    // If the height of the containing block is not specified explicitly (i.e., it depends on content height), and this element is not absolutely positioned,
    // the percentage value is treated as '0' (for 'min-height') or 'none' (for 'max-height').
    LayoutUnit minLogicalHeight;
    if (!logicalHeightComputesAsNone(MinSize))
        minLogicalHeight = computeReplacedLogicalHeightUsing(MinSize, style()->logicalMinHeight());
    LayoutUnit maxLogicalHeight = logicalHeight;
    if (!logicalHeightComputesAsNone(MaxSize))
        maxLogicalHeight =  computeReplacedLogicalHeightUsing(MaxSize, style()->logicalMaxHeight());
    return std::max(minLogicalHeight, std::min(logicalHeight, maxLogicalHeight));
}

LayoutUnit LayoutBox::computeReplacedLogicalHeightUsing(SizeType sizeType, const Length& logicalHeight) const
{
    ASSERT(sizeType == MinSize || sizeType == MainOrPreferredSize || !logicalHeight.isAuto());
    if (sizeType == MinSize && logicalHeight.isAuto())
        return adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit());

    switch (logicalHeight.type()) {
    case Fixed:
        return adjustContentBoxLogicalHeightForBoxSizing(logicalHeight.value());
    case Percent:
    case Calculated:
    {
        LayoutObject* cb = isOutOfFlowPositioned() ? container() : containingBlock();
        while (cb->isAnonymous())
            cb = cb->containingBlock();
        LayoutUnit stretchedFlexHeight(-1);
        if (cb->isLayoutBlock()) {
            LayoutBlock* block = toLayoutBlock(cb);
            block->addPercentHeightDescendant(const_cast<LayoutBox*>(this));
            if (block->isFlexItem())
                stretchedFlexHeight = toLayoutFlexibleBox(block->parent())->childLogicalHeightForPercentageResolution(*block);

        }

        if (cb->isOutOfFlowPositioned() && cb->style()->height().isAuto() && !(cb->style()->top().isAuto() || cb->style()->bottom().isAuto())) {
            ASSERT_WITH_SECURITY_IMPLICATION(cb->isLayoutBlock());
            LayoutBlock* block = toLayoutBlock(cb);
            LogicalExtentComputedValues computedValues;
            block->computeLogicalHeight(block->logicalHeight(), LayoutUnit(), computedValues);
            LayoutUnit newContentHeight = computedValues.m_extent - block->borderAndPaddingLogicalHeight() - block->scrollbarLogicalHeight();
            LayoutUnit newHeight = block->adjustContentBoxLogicalHeightForBoxSizing(newContentHeight);
            return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeight, newHeight));
        }

        // FIXME: availableLogicalHeight() is wrong if the replaced element's writing-mode is perpendicular to the
        // containing block's writing-mode.
        // https://bugs.webkit.org/show_bug.cgi?id=46496
        LayoutUnit availableHeight;
        if (isOutOfFlowPositioned()) {
            availableHeight = containingBlockLogicalHeightForPositioned(toLayoutBoxModelObject(cb));
        } else if (stretchedFlexHeight != -1) {
            availableHeight = stretchedFlexHeight;
        } else {
            availableHeight = containingBlockLogicalHeightForContent(IncludeMarginBorderPadding);
            // It is necessary to use the border-box to match WinIE's broken
            // box model.  This is essential for sizing inside
            // table cells using percentage heights.
            // FIXME: This needs to be made writing-mode-aware. If the cell and image are perpendicular writing-modes, this isn't right.
            // https://bugs.webkit.org/show_bug.cgi?id=46997
            while (cb && !cb->isLayoutView() && (cb->style()->logicalHeight().isAuto() || cb->style()->logicalHeight().hasPercent())) {
                if (cb->isTableCell()) {
                    // Don't let table cells squeeze percent-height replaced elements
                    // <http://bugs.webkit.org/show_bug.cgi?id=15359>
                    availableHeight = std::max(availableHeight, intrinsicLogicalHeight());
                    return valueForLength(logicalHeight, availableHeight - borderAndPaddingLogicalHeight());
                }
                toLayoutBlock(cb)->addPercentHeightDescendant(const_cast<LayoutBox*>(this));
                cb = cb->containingBlock();
            }
        }
        return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeight, availableHeight));
    }
    case MinContent:
    case MaxContent:
    case FitContent:
    case FillAvailable:
        return adjustContentBoxLogicalHeightForBoxSizing(computeIntrinsicLogicalContentHeightUsing(logicalHeight, intrinsicLogicalHeight(), borderAndPaddingHeight()));
    default:
        return intrinsicLogicalHeight();
    }
}

LayoutUnit LayoutBox::availableLogicalHeight(AvailableLogicalHeightType heightType) const
{
    // http://www.w3.org/TR/CSS2/visudet.html#propdef-height - We are interested in the content height.
    // FIXME: Should we pass intrinsicContentLogicalHeight() instead of -1 here?
    return constrainContentBoxLogicalHeightByMinMax(availableLogicalHeightUsing(style()->logicalHeight(), heightType), LayoutUnit(-1));
}

LayoutUnit LayoutBox::availableLogicalHeightUsing(const Length& h, AvailableLogicalHeightType heightType) const
{
    if (isLayoutView()) {
        return LayoutUnit(isHorizontalWritingMode()
            ? toLayoutView(this)->frameView()->visibleContentSize().height()
            : toLayoutView(this)->frameView()->visibleContentSize().width());
    }

    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isAuto() || h.hasPercent())) {
        if (hasOverrideLogicalContentHeight())
            return overrideLogicalContentHeight();
        return logicalHeight() - borderAndPaddingLogicalHeight();
    }

    if (h.hasPercent() && isOutOfFlowPositioned()) {
        // FIXME: This is wrong if the containingBlock has a perpendicular writing mode.
        LayoutUnit availableHeight = containingBlockLogicalHeightForPositioned(containingBlock());
        return adjustContentBoxLogicalHeightForBoxSizing(valueForLength(h, availableHeight));
    }

    // FIXME: Should we pass intrinsicContentLogicalHeight() instead of -1 here?
    LayoutUnit heightIncludingScrollbar = computeContentAndScrollbarLogicalHeightUsing(MainOrPreferredSize, h, LayoutUnit(-1));
    if (heightIncludingScrollbar != -1)
        return std::max(LayoutUnit(), adjustContentBoxLogicalHeightForBoxSizing(heightIncludingScrollbar) - scrollbarLogicalHeight());

    // FIXME: Check logicalTop/logicalBottom here to correctly handle vertical writing-mode.
    // https://bugs.webkit.org/show_bug.cgi?id=46500
    if (isLayoutBlock() && isOutOfFlowPositioned() && style()->height().isAuto() && !(style()->top().isAuto() || style()->bottom().isAuto())) {
        LayoutBlock* block = const_cast<LayoutBlock*>(toLayoutBlock(this));
        LogicalExtentComputedValues computedValues;
        block->computeLogicalHeight(block->logicalHeight(), LayoutUnit(), computedValues);
        LayoutUnit newContentHeight = computedValues.m_extent - block->borderAndPaddingLogicalHeight() - block->scrollbarLogicalHeight();
        return adjustContentBoxLogicalHeightForBoxSizing(newContentHeight);
    }

    // FIXME: This is wrong if the containingBlock has a perpendicular writing mode.
    LayoutUnit availableHeight = containingBlockLogicalHeightForContent(heightType);
    if (heightType == ExcludeMarginBorderPadding) {
        // FIXME: Margin collapsing hasn't happened yet, so this incorrectly removes collapsed margins.
        availableHeight -= marginBefore() + marginAfter() + borderAndPaddingLogicalHeight();
    }
    return availableHeight;
}

void LayoutBox::computeAndSetBlockDirectionMargins(const LayoutBlock* containingBlock)
{
    LayoutUnit marginBefore;
    LayoutUnit marginAfter;
    computeMarginsForDirection(BlockDirection, containingBlock, containingBlockLogicalWidthForContent(), logicalHeight(), marginBefore, marginAfter,
        style()->marginBeforeUsing(containingBlock->style()),
        style()->marginAfterUsing(containingBlock->style()));
    // Note that in this 'positioning phase' of the layout we are using the containing block's writing mode rather than our own when calculating margins.
    // See http://www.w3.org/TR/2014/CR-css-writing-modes-3-20140320/#orthogonal-flows
    containingBlock->setMarginBeforeForChild(*this, marginBefore);
    containingBlock->setMarginAfterForChild(*this, marginAfter);
}

LayoutUnit LayoutBox::containingBlockLogicalWidthForPositioned(const LayoutBoxModelObject* containingBlock, bool checkForPerpendicularWritingMode) const
{
    if (checkForPerpendicularWritingMode && containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalHeightForPositioned(containingBlock, false);

    // Use viewport as container for top-level fixed-position elements.
    if (style()->position() == FixedPosition && containingBlock->isLayoutView() && !document().printing()) {
        const LayoutView* view = toLayoutView(containingBlock);
        if (FrameView* frameView = view->frameView()) {
            // Don't use visibleContentRect since the PaintLayer's size has not been set yet.
            LayoutSize viewportSize(frameView->layoutViewportScrollableArea()->excludeScrollbars(frameView->frameRect().size()));
            return LayoutUnit(containingBlock->isHorizontalWritingMode() ? viewportSize.width() : viewportSize.height());
        }
    }

    if (hasOverrideContainingBlockLogicalWidth())
        return overrideContainingBlockContentLogicalWidth();

    // Ensure we compute our width based on the width of our rel-pos inline container rather than any anonymous block
    // created to manage a block-flow ancestor of ours in the rel-pos inline's inline flow.
    if (containingBlock->isAnonymousBlock() && containingBlock->isRelPositioned())
        containingBlock = toLayoutBox(containingBlock)->continuation();
    else if (containingBlock->isBox())
        return std::max(LayoutUnit(), toLayoutBox(containingBlock)->clientLogicalWidth());

    ASSERT(containingBlock->isLayoutInline() && containingBlock->isInFlowPositioned());

    const LayoutInline* flow = toLayoutInline(containingBlock);
    InlineFlowBox* first = flow->firstLineBox();
    InlineFlowBox* last = flow->lastLineBox();

    // If the containing block is empty, return a width of 0.
    if (!first || !last)
        return LayoutUnit();

    LayoutUnit fromLeft;
    LayoutUnit fromRight;
    if (containingBlock->style()->isLeftToRightDirection()) {
        fromLeft = first->logicalLeft() + first->borderLogicalLeft();
        fromRight = last->logicalLeft() + last->logicalWidth() - last->borderLogicalRight();
    } else {
        fromRight = first->logicalLeft() + first->logicalWidth() - first->borderLogicalRight();
        fromLeft = last->logicalLeft() + last->borderLogicalLeft();
    }

    return std::max(LayoutUnit(), fromRight - fromLeft);
}

LayoutUnit LayoutBox::containingBlockLogicalHeightForPositioned(const LayoutBoxModelObject* containingBlock, bool checkForPerpendicularWritingMode) const
{
    if (checkForPerpendicularWritingMode && containingBlock->isHorizontalWritingMode() != isHorizontalWritingMode())
        return containingBlockLogicalWidthForPositioned(containingBlock, false);

    // Use viewport as container for top-level fixed-position elements.
    if (style()->position() == FixedPosition && containingBlock->isLayoutView() && !document().printing()) {
        const LayoutView* view = toLayoutView(containingBlock);
        if (FrameView* frameView = view->frameView()) {
            // Don't use visibleContentRect since the PaintLayer's size has not been set yet.
            LayoutSize viewportSize(frameView->layoutViewportScrollableArea()->excludeScrollbars(frameView->frameRect().size()));
            return containingBlock->isHorizontalWritingMode() ? viewportSize.height() : viewportSize.width();
        }
    }

    if (hasOverrideContainingBlockLogicalHeight())
        return overrideContainingBlockContentLogicalHeight();

    if (containingBlock->isBox()) {
        const LayoutBlock* cb = containingBlock->isLayoutBlock() ?
            toLayoutBlock(containingBlock) : containingBlock->containingBlock();
        return cb->clientLogicalHeight();
    }

    ASSERT(containingBlock->isLayoutInline() && containingBlock->isInFlowPositioned());

    const LayoutInline* flow = toLayoutInline(containingBlock);
    InlineFlowBox* first = flow->firstLineBox();
    InlineFlowBox* last = flow->lastLineBox();

    // If the containing block is empty, return a height of 0.
    if (!first || !last)
        return LayoutUnit();

    LayoutUnit heightResult;
    LayoutRect boundingBox(flow->linesBoundingBox());
    if (containingBlock->isHorizontalWritingMode())
        heightResult = boundingBox.height();
    else
        heightResult = boundingBox.width();
    heightResult -= (containingBlock->borderBefore() + containingBlock->borderAfter());
    return heightResult;
}

void LayoutBox::computeInlineStaticDistance(Length& logicalLeft, Length& logicalRight, const LayoutBox* child, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth)
{
    if (!logicalLeft.isAuto() || !logicalRight.isAuto())
        return;

    // FIXME: The static distance computation has not been patched for mixed writing modes yet.
    if (child->parent()->style()->direction() == LTR) {
        LayoutUnit staticPosition = child->layer()->staticInlinePosition() - containerBlock->borderLogicalLeft();
        for (LayoutObject* curr = child->parent(); curr && curr != containerBlock; curr = curr->container()) {
            if (curr->isBox()) {
                staticPosition += toLayoutBox(curr)->logicalLeft();
                if (toLayoutBox(curr)->isInFlowPositioned())
                    staticPosition += toLayoutBox(curr)->offsetForInFlowPosition().width();
            } else if (curr->isInline()) {
                if (curr->isInFlowPositioned()) {
                    if (!curr->style()->logicalLeft().isAuto())
                        staticPosition += valueForLength(curr->style()->logicalLeft(), curr->containingBlock()->availableWidth());
                    else
                        staticPosition -= valueForLength(curr->style()->logicalRight(), curr->containingBlock()->availableWidth());
                }
            }
        }
        logicalLeft.setValue(Fixed, staticPosition);
    } else {
        LayoutBox* enclosingBox = child->parent()->enclosingBox();
        LayoutUnit staticPosition = child->layer()->staticInlinePosition() + containerLogicalWidth + containerBlock->borderLogicalLeft();
        for (LayoutObject* curr = child->parent(); curr; curr = curr->container()) {
            if (curr->isBox()) {
                if (curr != containerBlock) {
                    staticPosition -= toLayoutBox(curr)->logicalLeft();
                    if (toLayoutBox(curr)->isInFlowPositioned())
                        staticPosition -= toLayoutBox(curr)->offsetForInFlowPosition().width();
                }
                if (curr == enclosingBox)
                    staticPosition -= enclosingBox->logicalWidth();
            } else if (curr->isInline()) {
                if (curr->isInFlowPositioned()) {
                    if (!curr->style()->logicalLeft().isAuto())
                        staticPosition -= valueForLength(curr->style()->logicalLeft(), curr->containingBlock()->availableWidth());
                    else
                        staticPosition += valueForLength(curr->style()->logicalRight(), curr->containingBlock()->availableWidth());
                }
            }
            if (curr == containerBlock)
                break;
        }
        logicalRight.setValue(Fixed, staticPosition);
    }
}

void LayoutBox::computePositionedLogicalWidth(LogicalExtentComputedValues& computedValues) const
{
    // QUESTIONS
    // FIXME 1: Should we still deal with these the cases of 'left' or 'right' having
    // the type 'static' in determining whether to calculate the static distance?
    // NOTE: 'static' is not a legal value for 'left' or 'right' as of CSS 2.1.

    // FIXME 2: Can perhaps optimize out cases when max-width/min-width are greater
    // than or less than the computed width().  Be careful of box-sizing and
    // percentage issues.

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.7 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width>
    // (block-style-comments in this function and in computePositionedLogicalWidthUsing()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const LayoutBoxModelObject* containerBlock = toLayoutBoxModelObject(container());

    const LayoutUnit containerLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock);

    // Use the container block's direction except when calculating the static distance
    // This conforms with the reference results for abspos-replaced-width-margin-000.htm
    // of the CSS 2.1 test suite
    TextDirection containerDirection = containerBlock->style()->direction();

    bool isHorizontal = isHorizontalWritingMode();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalWidth();
    const Length marginLogicalLeft = isHorizontal ? style()->marginLeft() : style()->marginTop();
    const Length marginLogicalRight = isHorizontal ? style()->marginRight() : style()->marginBottom();

    Length logicalLeftLength = style()->logicalLeft();
    Length logicalRightLength = style()->logicalRight();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely:
     *
     * * The static position for 'left' is the distance from the left edge of the
     *   containing block to the left margin edge of a hypothetical box that would
     *   have been the first box of the element if its 'position' property had
     *   been 'static' and 'float' had been 'none'. The value is negative if the
     *   hypothetical box is to the left of the containing block.
     * * The static position for 'right' is the distance from the right edge of the
     *   containing block to the right margin edge of the same hypothetical box as
     *   above. The value is positive if the hypothetical box is to the left of the
     *   containing block's edge.
     *
     * But rather than actually calculating the dimensions of that hypothetical box,
     * user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block of
     * fixed positioned elements is the initial containing block instead of the
     * viewport, and all scrollable boxes should be assumed to be scrolled to their
     * origin.
    \*---------------------------------------------------------------------------*/

    // see FIXME 1
    // Calculate the static distance if needed.
    computeInlineStaticDistance(logicalLeftLength, logicalRightLength, this, containerBlock, containerLogicalWidth);

    // Calculate constraint equation values for 'width' case.
    computePositionedLogicalWidthUsing(MainOrPreferredSize, style()->logicalWidth(), containerBlock, containerDirection,
        containerLogicalWidth, bordersPlusPadding,
        logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
        computedValues);

    // Calculate constraint equation values for 'max-width' case.
    if (!style()->logicalMaxWidth().isMaxSizeNone()) {
        LogicalExtentComputedValues maxValues;

        computePositionedLogicalWidthUsing(MaxSize, style()->logicalMaxWidth(), containerBlock, containerDirection,
            containerLogicalWidth, bordersPlusPadding,
            logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
            maxValues);

        if (computedValues.m_extent > maxValues.m_extent) {
            computedValues.m_extent = maxValues.m_extent;
            computedValues.m_position = maxValues.m_position;
            computedValues.m_margins.m_start = maxValues.m_margins.m_start;
            computedValues.m_margins.m_end = maxValues.m_margins.m_end;
        }
    }

    // Calculate constraint equation values for 'min-width' case.
    if (!style()->logicalMinWidth().isZero() || style()->logicalMinWidth().isIntrinsic()) {
        LogicalExtentComputedValues minValues;

        computePositionedLogicalWidthUsing(MinSize, style()->logicalMinWidth(), containerBlock, containerDirection,
            containerLogicalWidth, bordersPlusPadding,
            logicalLeftLength, logicalRightLength, marginLogicalLeft, marginLogicalRight,
            minValues);

        if (computedValues.m_extent < minValues.m_extent) {
            computedValues.m_extent = minValues.m_extent;
            computedValues.m_position = minValues.m_position;
            computedValues.m_margins.m_start = minValues.m_margins.m_start;
            computedValues.m_margins.m_end = minValues.m_margins.m_end;
        }
    }

    if (!style()->hasStaticInlinePosition(isHorizontal))
        computedValues.m_position += extraInlineOffset();

    computedValues.m_extent += bordersPlusPadding;
}

void LayoutBox::computeLogicalLeftPositionedOffset(LayoutUnit& logicalLeftPos, const LayoutBox* child, LayoutUnit logicalWidthValue, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalWidth)
{
    // Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate.  This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if (containerBlock->isHorizontalWritingMode() != child->isHorizontalWritingMode() && containerBlock->style()->isFlippedBlocksWritingMode()) {
        logicalLeftPos = containerLogicalWidth - logicalWidthValue - logicalLeftPos;
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock->borderRight() : containerBlock->borderBottom());
    } else {
        logicalLeftPos += (child->isHorizontalWritingMode() ? containerBlock->borderLeft() : containerBlock->borderTop());
    }
}

LayoutUnit LayoutBox::shrinkToFitLogicalWidth(LayoutUnit availableLogicalWidth, LayoutUnit bordersPlusPadding) const
{
    LayoutUnit preferredLogicalWidth = maxPreferredLogicalWidth() - bordersPlusPadding;
    LayoutUnit preferredMinLogicalWidth = minPreferredLogicalWidth() - bordersPlusPadding;
    return std::min(std::max(preferredMinLogicalWidth, availableLogicalWidth), preferredLogicalWidth);
}

void LayoutBox::computePositionedLogicalWidthUsing(SizeType widthSizeType, Length logicalWidth, const LayoutBoxModelObject* containerBlock, TextDirection containerDirection,
    LayoutUnit containerLogicalWidth, LayoutUnit bordersPlusPadding,
    const Length& logicalLeft, const Length& logicalRight, const Length& marginLogicalLeft,
    const Length& marginLogicalRight, LogicalExtentComputedValues& computedValues) const
{
    LayoutUnit logicalWidthValue;

    ASSERT(widthSizeType == MinSize || widthSizeType == MainOrPreferredSize || !logicalWidth.isAuto());
    if (widthSizeType == MinSize && logicalWidth.isAuto())
        logicalWidthValue = LayoutUnit();
    else if (logicalWidth.isIntrinsic())
        logicalWidthValue = computeIntrinsicLogicalWidthUsing(logicalWidth, containerLogicalWidth, bordersPlusPadding) - bordersPlusPadding;
    else
        logicalWidthValue = adjustContentBoxLogicalWidthForBoxSizing(valueForLength(logicalWidth, containerLogicalWidth));

    // 'left' and 'right' cannot both be 'auto' because one would of been
    // converted to the static position already
    ASSERT(!(logicalLeft.isAuto() && logicalRight.isAuto()));

    // minimumValueForLength will convert 'auto' to 0 so that it doesn't impact the available space computation below.
    LayoutUnit logicalLeftValue = minimumValueForLength(logicalLeft, containerLogicalWidth);
    LayoutUnit logicalRightValue = minimumValueForLength(logicalRight, containerLogicalWidth);

    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, false);

    bool logicalWidthIsAuto = logicalWidth.isAuto();
    bool logicalLeftIsAuto = logicalLeft.isAuto();
    bool logicalRightIsAuto = logicalRight.isAuto();
    LayoutUnit& marginLogicalLeftValue = style()->isLeftToRightDirection() ? computedValues.m_margins.m_start : computedValues.m_margins.m_end;
    LayoutUnit& marginLogicalRightValue = style()->isLeftToRightDirection() ? computedValues.m_margins.m_end : computedValues.m_margins.m_start;
    if (!logicalLeftIsAuto && !logicalWidthIsAuto && !logicalRightIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three is 'auto': If both 'margin-left' and 'margin-
         * right' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values, unless this would make them negative,
         * in which case when direction of the containing block is 'ltr' ('rtl'),
         * set 'margin-left' ('margin-right') to zero and solve for 'margin-right'
         * ('margin-left'). If one of 'margin-left' or 'margin-right' is 'auto',
         * solve the equation for that value. If the values are over-constrained,
         * ignore the value for 'left' (in case the 'direction' property of the
         * containing block is 'rtl') or 'right' (in case 'direction' is 'ltr')
         * and solve for that value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'right' in the over constrained
        // case because the value is not used for any further calculations.

        computedValues.m_extent = logicalWidthValue;

        const LayoutUnit availableSpace = containerLogicalWidth - (logicalLeftValue + computedValues.m_extent + logicalRightValue + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginLogicalLeft.isAuto() && marginLogicalRight.isAuto()) {
            // Both margins auto, solve for equality
            if (availableSpace >= 0) {
                marginLogicalLeftValue = availableSpace / 2; // split the difference
                marginLogicalRightValue = availableSpace - marginLogicalLeftValue; // account for odd valued differences
            } else {
                // Use the containing block's direction rather than the parent block's
                // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
                if (containerDirection == LTR) {
                    marginLogicalLeftValue = LayoutUnit();
                    marginLogicalRightValue = availableSpace; // will be negative
                } else {
                    marginLogicalLeftValue = availableSpace; // will be negative
                    marginLogicalRightValue = LayoutUnit();
                }
            }
        } else if (marginLogicalLeft.isAuto()) {
            // Solve for left margin
            marginLogicalRightValue = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);
            marginLogicalLeftValue = availableSpace - marginLogicalRightValue;
        } else if (marginLogicalRight.isAuto()) {
            // Solve for right margin
            marginLogicalLeftValue = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
            marginLogicalRightValue = availableSpace - marginLogicalLeftValue;
        } else {
            // Over-constrained, solve for left if direction is RTL
            marginLogicalLeftValue = valueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
            marginLogicalRightValue = valueForLength(marginLogicalRight, containerRelativeLogicalWidth);

            // Use the containing block's direction rather than the parent block's
            // per CSS 2.1 reference test abspos-non-replaced-width-margin-000.
            if (containerDirection == RTL)
                logicalLeftValue = (availableSpace + logicalLeftValue) - marginLogicalLeftValue - marginLogicalRightValue;
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-left' and 'margin-right'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the
         *    width is shrink-to-fit. Then solve for 'left'
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if
         *    the 'direction' property of the containing block is 'ltr' set
         *    'left' to the static position, otherwise set 'right' to the
         *    static position. Then solve for 'left' (if 'direction is 'rtl')
         *    or 'right' (if 'direction' is 'ltr').
         * ------------------------------------------------------------------
         *
         * 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the
         *    width is shrink-to-fit . Then solve for 'right'
         * 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve
         *    for 'left'
         * 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve
         *    for 'width'
         * 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve
         *    for 'right'
         *
         * Calculation of the shrink-to-fit width is similar to calculating the
         * width of a table cell using the automatic table layout algorithm.
         * Roughly: calculate the preferred width by formatting the content
         * without breaking lines other than where explicit line breaks occur,
         * and also calculate the preferred minimum width, e.g., by trying all
         * possible line breaks. CSS 2.1 does not define the exact algorithm.
         * Thirdly, calculate the available width: this is found by solving
         * for 'width' after setting 'left' (in case 1) or 'right' (in case 3)
         * to 0.
         *
         * Then the shrink-to-fit width is:
         * min(max(preferred minimum width, available width), preferred width).
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'right'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        marginLogicalLeftValue = minimumValueForLength(marginLogicalLeft, containerRelativeLogicalWidth);
        marginLogicalRightValue = minimumValueForLength(marginLogicalRight, containerRelativeLogicalWidth);

        const LayoutUnit availableSpace = containerLogicalWidth - (marginLogicalLeftValue + marginLogicalRightValue + logicalLeftValue + logicalRightValue + bordersPlusPadding);

        // FIXME: Is there a faster way to find the correct case?
        // Use rule/case that applies.
        if (logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 1: (use shrink-to-fit for width, and solve of left)
            computedValues.m_extent = shrinkToFitLogicalWidth(availableSpace, bordersPlusPadding);
            logicalLeftValue = availableSpace - computedValues.m_extent;
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 3: (use shrink-to-fit for width, and no need solve of right)
            computedValues.m_extent = shrinkToFitLogicalWidth(availableSpace, bordersPlusPadding);
        } else if (logicalLeftIsAuto && !logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 4: (solve for left)
            computedValues.m_extent = logicalWidthValue;
            logicalLeftValue = availableSpace - computedValues.m_extent;
        } else if (!logicalLeftIsAuto && logicalWidthIsAuto && !logicalRightIsAuto) {
            // RULE 5: (solve for width)
            if (autoWidthShouldFitContent())
                computedValues.m_extent = shrinkToFitLogicalWidth(availableSpace, bordersPlusPadding);
            else
                computedValues.m_extent = std::max(LayoutUnit(), availableSpace);
        } else if (!logicalLeftIsAuto && !logicalWidthIsAuto && logicalRightIsAuto) {
            // RULE 6: (no need solve for right)
            computedValues.m_extent = logicalWidthValue;
        }
    }

    // Use computed values to calculate the horizontal position.

    // FIXME: This hack is needed to calculate the  logical left position for a 'rtl' relatively
    // positioned, inline because right now, it is using the logical left position
    // of the first line box when really it should use the last line box.  When
    // this is fixed elsewhere, this block should be removed.
    if (containerBlock->isLayoutInline() && !containerBlock->style()->isLeftToRightDirection()) {
        const LayoutInline* flow = toLayoutInline(containerBlock);
        InlineFlowBox* firstLine = flow->firstLineBox();
        InlineFlowBox* lastLine = flow->lastLineBox();
        if (firstLine && lastLine && firstLine != lastLine) {
            computedValues.m_position = logicalLeftValue + marginLogicalLeftValue + lastLine->borderLogicalLeft() + (lastLine->logicalLeft() - firstLine->logicalLeft());
            return;
        }
    }

    if (containerBlock->isBox() && toLayoutBox(containerBlock)->scrollsOverflowY() && toLayoutBox(containerBlock)->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        logicalLeftValue = logicalLeftValue + toLayoutBox(containerBlock)->verticalScrollbarWidth();
    }

    computedValues.m_position = logicalLeftValue + marginLogicalLeftValue;
    computeLogicalLeftPositionedOffset(computedValues.m_position, this, computedValues.m_extent, containerBlock, containerLogicalWidth);
}

void LayoutBox::computeBlockStaticDistance(Length& logicalTop, Length& logicalBottom, const LayoutBox* child, const LayoutBoxModelObject* containerBlock)
{
    if (!logicalTop.isAuto() || !logicalBottom.isAuto())
        return;

    // FIXME: The static distance computation has not been patched for mixed writing modes.
    LayoutUnit staticLogicalTop = child->layer()->staticBlockPosition() - containerBlock->borderBefore();
    for (LayoutObject* curr = child->parent(); curr && curr != containerBlock; curr = curr->container()) {
        if (curr->isBox() && !curr->isTableRow())
            staticLogicalTop += toLayoutBox(curr)->logicalTop();
    }
    logicalTop.setValue(Fixed, staticLogicalTop);
}

void LayoutBox::computePositionedLogicalHeight(LogicalExtentComputedValues& computedValues) const
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.4 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-non-replaced-height>
    // (block-style-comments in this function and in computePositionedLogicalHeightUsing()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const LayoutBoxModelObject* containerBlock = toLayoutBoxModelObject(container());

    const LayoutUnit containerLogicalHeight = containingBlockLogicalHeightForPositioned(containerBlock);

    const ComputedStyle& styleToUse = styleRef();
    const LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    const Length marginBefore = styleToUse.marginBefore();
    const Length marginAfter = styleToUse.marginAfter();
    Length logicalTopLength = styleToUse.logicalTop();
    Length logicalBottomLength = styleToUse.logicalBottom();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely, the static position for 'top' is the
     * distance from the top edge of the containing block to the top margin edge
     * of a hypothetical box that would have been the first box of the element if
     * its 'position' property had been 'static' and 'float' had been 'none'. The
     * value is negative if the hypothetical box is above the containing block.
     *
     * But rather than actually calculating the dimensions of that hypothetical
     * box, user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block
     * of fixed positioned elements is the initial containing block instead of
     * the viewport.
    \*---------------------------------------------------------------------------*/

    // see FIXME 1
    // Calculate the static distance if needed.
    computeBlockStaticDistance(logicalTopLength, logicalBottomLength, this, containerBlock);

    // Calculate constraint equation values for 'height' case.
    LayoutUnit logicalHeight = computedValues.m_extent;
    computePositionedLogicalHeightUsing(MainOrPreferredSize, styleToUse.logicalHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
        logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
        computedValues);

    // Avoid doing any work in the common case (where the values of min-height and max-height are their defaults).
    // see FIXME 2

    // Calculate constraint equation values for 'max-height' case.
    if (!styleToUse.logicalMaxHeight().isMaxSizeNone()) {
        LogicalExtentComputedValues maxValues;

        computePositionedLogicalHeightUsing(MaxSize, styleToUse.logicalMaxHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
            maxValues);

        if (computedValues.m_extent > maxValues.m_extent) {
            computedValues.m_extent = maxValues.m_extent;
            computedValues.m_position = maxValues.m_position;
            computedValues.m_margins.m_before = maxValues.m_margins.m_before;
            computedValues.m_margins.m_after = maxValues.m_margins.m_after;
        }
    }

    // Calculate constraint equation values for 'min-height' case.
    if (!styleToUse.logicalMinHeight().isZero() || styleToUse.logicalMinHeight().isIntrinsic()) {
        LogicalExtentComputedValues minValues;

        computePositionedLogicalHeightUsing(MinSize, styleToUse.logicalMinHeight(), containerBlock, containerLogicalHeight, bordersPlusPadding, logicalHeight,
            logicalTopLength, logicalBottomLength, marginBefore, marginAfter,
            minValues);

        if (computedValues.m_extent < minValues.m_extent) {
            computedValues.m_extent = minValues.m_extent;
            computedValues.m_position = minValues.m_position;
            computedValues.m_margins.m_before = minValues.m_margins.m_before;
            computedValues.m_margins.m_after = minValues.m_margins.m_after;
        }
    }

    if (!style()->hasStaticBlockPosition(isHorizontalWritingMode()))
        computedValues.m_position += extraBlockOffset();

    // Set final height value.
    computedValues.m_extent += bordersPlusPadding;
}

void LayoutBox::computeLogicalTopPositionedOffset(LayoutUnit& logicalTopPos, const LayoutBox* child, LayoutUnit logicalHeightValue, const LayoutBoxModelObject* containerBlock, LayoutUnit containerLogicalHeight)
{
    // Deal with differing writing modes here.  Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate.  This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if ((child->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() != containerBlock->isHorizontalWritingMode())
        || (child->style()->isFlippedBlocksWritingMode() != containerBlock->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() == containerBlock->isHorizontalWritingMode()))
        logicalTopPos = containerLogicalHeight - logicalHeightValue - logicalTopPos;

    // Our offset is from the logical bottom edge in a flipped environment, e.g., right for vertical-rl.
    if (containerBlock->style()->isFlippedBlocksWritingMode() && child->isHorizontalWritingMode() == containerBlock->isHorizontalWritingMode()) {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock->borderBottom();
        else
            logicalTopPos += containerBlock->borderRight();
    } else {
        if (child->isHorizontalWritingMode())
            logicalTopPos += containerBlock->borderTop();
        else
            logicalTopPos += containerBlock->borderLeft();
    }
}

void LayoutBox::computePositionedLogicalHeightUsing(SizeType heightSizeType, Length logicalHeightLength, const LayoutBoxModelObject* containerBlock,
    LayoutUnit containerLogicalHeight, LayoutUnit bordersPlusPadding, LayoutUnit logicalHeight,
    const Length& logicalTop, const Length& logicalBottom, const Length& marginBefore,
    const Length& marginAfter, LogicalExtentComputedValues& computedValues) const
{
    ASSERT(heightSizeType == MinSize || heightSizeType == MainOrPreferredSize || !logicalHeightLength.isAuto());
    if (heightSizeType == MinSize && logicalHeightLength.isAuto())
        logicalHeightLength = Length(0, Fixed);

    // 'top' and 'bottom' cannot both be 'auto' because 'top would of been
    // converted to the static position in computePositionedLogicalHeight()
    ASSERT(!(logicalTop.isAuto() && logicalBottom.isAuto()));

    LayoutUnit logicalHeightValue;
    LayoutUnit contentLogicalHeight = logicalHeight - bordersPlusPadding;

    const LayoutUnit containerRelativeLogicalWidth = containingBlockLogicalWidthForPositioned(containerBlock, false);

    LayoutUnit logicalTopValue;

    bool logicalHeightIsAuto = logicalHeightLength.isAuto();
    bool logicalTopIsAuto = logicalTop.isAuto();
    bool logicalBottomIsAuto = logicalBottom.isAuto();

    LayoutUnit resolvedLogicalHeight;
    // Height is never unsolved for tables.
    if (isTable()) {
        resolvedLogicalHeight = contentLogicalHeight;
        logicalHeightIsAuto = false;
    } else {
        if (logicalHeightLength.isIntrinsic())
            resolvedLogicalHeight = computeIntrinsicLogicalContentHeightUsing(logicalHeightLength, contentLogicalHeight, bordersPlusPadding);
        else
            resolvedLogicalHeight = adjustContentBoxLogicalHeightForBoxSizing(valueForLength(logicalHeightLength, containerLogicalHeight));
    }

    if (!logicalTopIsAuto && !logicalHeightIsAuto && !logicalBottomIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three are 'auto': If both 'margin-top' and 'margin-
         * bottom' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values. If one of 'margin-top' or 'margin-
         * bottom' is 'auto', solve the equation for that value. If the values
         * are over-constrained, ignore the value for 'bottom' and solve for that
         * value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'bottom' in the over constrained
        // case because the value is not used for any further calculations.

        logicalHeightValue = resolvedLogicalHeight;
        logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);

        const LayoutUnit availableSpace = containerLogicalHeight - (logicalTopValue + logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginBefore.isAuto() && marginAfter.isAuto()) {
            // Both margins auto, solve for equality
            // NOTE: This may result in negative values.
            computedValues.m_margins.m_before = availableSpace / 2; // split the difference
            computedValues.m_margins.m_after = availableSpace - computedValues.m_margins.m_before; // account for odd valued differences
        } else if (marginBefore.isAuto()) {
            // Solve for top margin
            computedValues.m_margins.m_after = valueForLength(marginAfter, containerRelativeLogicalWidth);
            computedValues.m_margins.m_before = availableSpace - computedValues.m_margins.m_after;
        } else if (marginAfter.isAuto()) {
            // Solve for bottom margin
            computedValues.m_margins.m_before = valueForLength(marginBefore, containerRelativeLogicalWidth);
            computedValues.m_margins.m_after = availableSpace - computedValues.m_margins.m_before;
        } else {
            // Over-constrained, (no need solve for bottom)
            computedValues.m_margins.m_before = valueForLength(marginBefore, containerRelativeLogicalWidth);
            computedValues.m_margins.m_after = valueForLength(marginAfter, containerRelativeLogicalWidth);
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-top' and 'margin-bottom'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then
         *    the height is based on the content, and solve for 'top'.
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then
         *    set 'top' to the static position, and solve for 'bottom'.
         * ------------------------------------------------------------------
         *
         * 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then
         *    the height is based on the content, and solve for 'bottom'.
         * 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', and
         *    solve for 'top'.
         * 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', and
         *    solve for 'height'.
         * 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', and
         *    solve for 'bottom'.
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'bottom'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        computedValues.m_margins.m_before = minimumValueForLength(marginBefore, containerRelativeLogicalWidth);
        computedValues.m_margins.m_after = minimumValueForLength(marginAfter, containerRelativeLogicalWidth);

        const LayoutUnit availableSpace = containerLogicalHeight - (computedValues.m_margins.m_before + computedValues.m_margins.m_after + bordersPlusPadding);

        // Use rule/case that applies.
        if (logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 1: (height is content based, solve of top)
            logicalHeightValue = contentLogicalHeight;
            logicalTopValue = availableSpace - (logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 3: (height is content based, no need solve of bottom)
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
            logicalHeightValue = contentLogicalHeight;
        } else if (logicalTopIsAuto && !logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 4: (solve of top)
            logicalHeightValue = resolvedLogicalHeight;
            logicalTopValue = availableSpace - (logicalHeightValue + valueForLength(logicalBottom, containerLogicalHeight));
        } else if (!logicalTopIsAuto && logicalHeightIsAuto && !logicalBottomIsAuto) {
            // RULE 5: (solve of height)
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
            logicalHeightValue = std::max(LayoutUnit(), availableSpace - (logicalTopValue + valueForLength(logicalBottom, containerLogicalHeight)));
        } else if (!logicalTopIsAuto && !logicalHeightIsAuto && logicalBottomIsAuto) {
            // RULE 6: (no need solve of bottom)
            logicalHeightValue = resolvedLogicalHeight;
            logicalTopValue = valueForLength(logicalTop, containerLogicalHeight);
        }
    }
    computedValues.m_extent = logicalHeightValue;

    // Use computed values to calculate the vertical position.
    computedValues.m_position = logicalTopValue + computedValues.m_margins.m_before;
    computeLogicalTopPositionedOffset(computedValues.m_position, this, logicalHeightValue, containerBlock, containerLogicalHeight);
}

LayoutRect LayoutBox::localCaretRect(InlineBox* box, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    // VisiblePositions at offsets inside containers either a) refer to the positions before/after
    // those containers (tables and select elements) or b) refer to the position inside an empty block.
    // They never refer to children.
    // FIXME: Paint the carets inside empty blocks differently than the carets before/after elements.

    LayoutRect rect(location(), LayoutSize(caretWidth(), size().height()));
    bool ltr = box ? box->isLeftToRightDirection() : style()->isLeftToRightDirection();

    if ((!caretOffset) ^ ltr)
        rect.move(LayoutSize(size().width() - caretWidth(), LayoutUnit()));

    if (box) {
        RootInlineBox& rootBox = box->root();
        LayoutUnit top = rootBox.lineTop();
        rect.setY(top);
        rect.setHeight(rootBox.lineBottom() - top);
    }

    // If height of box is smaller than font height, use the latter one,
    // otherwise the caret might become invisible.
    //
    // Also, if the box is not an atomic inline-level element, always use the font height.
    // This prevents the "big caret" bug described in:
    // <rdar://problem/3777804> Deleting all content in a document can result in giant tall-as-window insertion point
    //
    // FIXME: ignoring :first-line, missing good reason to take care of
    LayoutUnit fontHeight = LayoutUnit(style()->getFontMetrics().height());
    if (fontHeight > rect.height() || (!isAtomicInlineLevel() && !isTable()))
        rect.setHeight(fontHeight);

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = location().x() + size().width() - rect.maxX();

    // Move to local coords
    rect.moveBy(-location());

    // FIXME: Border/padding should be added for all elements but this workaround
    // is needed because we use offsets inside an "atomic" element to represent
    // positions before and after the element in deprecated editing offsets.
    if (node() && !(editingIgnoresContent(node()) || isDisplayInsideTable(node()))) {
        rect.setX(rect.x() + borderLeft() + paddingLeft());
        rect.setY(rect.y() + paddingTop() + borderTop());
    }

    if (!isHorizontalWritingMode())
        return rect.transposedRect();

    return rect;
}

PositionWithAffinity LayoutBox::positionForPoint(const LayoutPoint& point)
{
    // no children...return this layout object's element, if there is one, and offset 0
    LayoutObject* firstChild = slowFirstChild();
    if (!firstChild)
        return createPositionWithAffinity(nonPseudoNode() ? firstPositionInOrBeforeNode(nonPseudoNode()) : Position());

    if (isTable() && nonPseudoNode()) {
        LayoutUnit right = size().width() - verticalScrollbarWidth();
        LayoutUnit bottom = size().height() - horizontalScrollbarHeight();

        if (point.x() < 0 || point.x() > right || point.y() < 0 || point.y() > bottom) {
            if (point.x() <= right / 2)
                return createPositionWithAffinity(firstPositionInOrBeforeNode(nonPseudoNode()));
            return createPositionWithAffinity(lastPositionInOrAfterNode(nonPseudoNode()));
        }
    }

    // Pass off to the closest child.
    LayoutUnit minDist = LayoutUnit::max();
    LayoutBox* closestLayoutObject = nullptr;
    LayoutPoint adjustedPoint = point;
    if (isTableRow())
        adjustedPoint.moveBy(location());

    for (LayoutObject* layoutObject = firstChild; layoutObject; layoutObject = layoutObject->nextSibling()) {
        if ((!layoutObject->slowFirstChild() && !layoutObject->isInline() && !layoutObject->isLayoutBlockFlow() )
            || layoutObject->style()->visibility() != VISIBLE)
            continue;

        if (!layoutObject->isBox())
            continue;

        LayoutBox* layoutBox = toLayoutBox(layoutObject);

        LayoutUnit top = layoutBox->borderTop() + layoutBox->paddingTop() + (isTableRow() ? LayoutUnit() : layoutBox->location().y());
        LayoutUnit bottom = top + layoutBox->contentHeight();
        LayoutUnit left = layoutBox->borderLeft() + layoutBox->paddingLeft() + (isTableRow() ? LayoutUnit() : layoutBox->location().x());
        LayoutUnit right = left + layoutBox->contentWidth();

        if (point.x() <= right && point.x() >= left && point.y() <= top && point.y() >= bottom) {
            if (layoutBox->isTableRow())
                return layoutBox->positionForPoint(point + adjustedPoint - layoutBox->locationOffset());
            return layoutBox->positionForPoint(point - layoutBox->locationOffset());
        }

        // Find the distance from (x, y) to the box.  Split the space around the box into 8 pieces
        // and use a different compare depending on which piece (x, y) is in.
        LayoutPoint cmp;
        if (point.x() > right) {
            if (point.y() < top)
                cmp = LayoutPoint(right, top);
            else if (point.y() > bottom)
                cmp = LayoutPoint(right, bottom);
            else
                cmp = LayoutPoint(right, point.y());
        } else if (point.x() < left) {
            if (point.y() < top)
                cmp = LayoutPoint(left, top);
            else if (point.y() > bottom)
                cmp = LayoutPoint(left, bottom);
            else
                cmp = LayoutPoint(left, point.y());
        } else {
            if (point.y() < top)
                cmp = LayoutPoint(point.x(), top);
            else
                cmp = LayoutPoint(point.x(), bottom);
        }

        LayoutSize difference = cmp - point;

        LayoutUnit dist = difference.width() * difference.width() + difference.height() * difference.height();
        if (dist < minDist) {
            closestLayoutObject = layoutBox;
            minDist = dist;
        }
    }

    if (closestLayoutObject)
        return closestLayoutObject->positionForPoint(adjustedPoint - closestLayoutObject->locationOffset());
    return createPositionWithAffinity(firstPositionInOrBeforeNode(nonPseudoNode()));
}

bool LayoutBox::shrinkToAvoidFloats() const
{
    // Floating objects don't shrink.  Objects that don't avoid floats don't shrink.
    if (isInline() || !avoidsFloats() || isFloating())
        return false;

    // Only auto width objects can possibly shrink to avoid floats.
    return style()->width().isAuto();
}

static bool shouldBeConsideredAsReplaced(Node* node)
{
    // Checkboxes and radioboxes are not isAtomicInlineLevel() nor do they have their own layoutObject in which to override avoidFloats().
    return node && node->isElementNode() && (toElement(node)->isFormControlElement() || isHTMLImageElement(toElement(node)));
}

bool LayoutBox::avoidsFloats() const
{
    return isAtomicInlineLevel() || shouldBeConsideredAsReplaced(node()) || hasOverflowClip() || isHR() || isLegend() || isWritingModeRoot() || isFlexItemIncludingDeprecated() || style()->containsPaint() || style()->containsLayout();
}

bool LayoutBox::hasNonCompositedScrollbars() const
{
    if (PaintLayerScrollableArea* scrollableArea = this->getScrollableArea()) {
        if (scrollableArea->hasHorizontalScrollbar() && !scrollableArea->layerForHorizontalScrollbar())
            return true;
        if (scrollableArea->hasVerticalScrollbar() && !scrollableArea->layerForVerticalScrollbar())
            return true;
    }
    return false;
}

PaintInvalidationReason LayoutBox::getPaintInvalidationReason(const PaintInvalidationState& paintInvalidationState,
    const LayoutRect& oldBounds, const LayoutPoint& oldLocation, const LayoutRect& newBounds, const LayoutPoint& newLocation) const
{
    PaintInvalidationReason invalidationReason = LayoutBoxModelObject::getPaintInvalidationReason(paintInvalidationState, oldBounds, oldLocation, newBounds, newLocation);
    if (isFullPaintInvalidationReason(invalidationReason))
        return invalidationReason;

    if (isLayoutView()) {
        const LayoutView* layoutView = toLayoutView(this);
        // In normal compositing mode, root background doesn't need to be invalidated for
        // box changes, because the background always covers the whole document rect
        // and clipping is done by compositor()->m_containerLayer. Also the scrollbars
        // are always composited. There are no other box decoration on the LayoutView thus
        // we can safely exit here.
        if (layoutView->usesCompositing() && (!document().settings() || !document().settings()->rootLayerScrolls()))
            return invalidationReason;
    }

    // If the transform is not identity or translation, incremental invalidation is not applicable
    // because the difference between oldBounds and newBounds doesn't cover all area needing invalidation.
    // FIXME: Should also consider ancestor transforms since paintInvalidationContainer. crbug.com/426111.
    if (invalidationReason == PaintInvalidationIncremental
        && paintInvalidationState.paintInvalidationContainer() != this
        && hasLayer() && layer()->transform() && !layer()->transform()->isIdentityOrTranslation())
        return PaintInvalidationBoundsChange;

    if (style()->backgroundLayers().thisOrNextLayersUseContentBox() || style()->maskLayers().thisOrNextLayersUseContentBox() || style()->boxSizing() == BoxSizingBorderBox) {
        LayoutRect oldContentBoxRect = m_rareData ? m_rareData->m_previousContentBoxRect : LayoutRect();
        LayoutRect newContentBoxRect = contentBoxRect();
        if (oldContentBoxRect != newContentBoxRect)
            return PaintInvalidationContentBoxChange;
    }

    if (!style()->hasBackground() && !style()->hasBoxDecorations()) {
        // We could let incremental invalidation cover non-composited scrollbars, but just
        // do a full invalidation because incremental invalidation will go away with slimming paint.
        if (invalidationReason == PaintInvalidationIncremental && hasNonCompositedScrollbars())
            return PaintInvalidationBorderBoxChange;
        return invalidationReason;
    }

    if (style()->backgroundLayers().thisOrNextLayersHaveLocalAttachment()) {
        LayoutRect oldLayoutOverflowRect = m_rareData ? m_rareData->m_previousLayoutOverflowRect : LayoutRect();
        LayoutRect newLayoutOverflowRect = layoutOverflowRect();
        if (oldLayoutOverflowRect != newLayoutOverflowRect)
            return PaintInvalidationLayoutOverflowBoxChange;
    }

    LayoutSize oldBorderBoxSize = computePreviousBorderBoxSize(oldBounds.size());
    LayoutSize newBorderBoxSize = size();

    if (oldBorderBoxSize == newBorderBoxSize)
        return invalidationReason;

    // LayoutBox::incrementallyInvalidatePaint() depends on positionFromPaintInvalidationBacking
    // which is not available when slimmingPaintOffsetCachingEnabled.
    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() && (style()->hasBoxDecorations() || style()->hasBackground()))
        return PaintInvalidationBorderBoxChange;

    // TODO(wangxianzhu): Remove incremental invalidation when we remove rect-based paint invalidation.
    // See another hasNonCompositedScrollbars() callsite above.
    if (hasNonCompositedScrollbars())
        return PaintInvalidationBorderBoxChange;

    if (style()->hasVisualOverflowingEffect() || style()->hasAppearance() || style()->hasFilterInducingProperty() || style()->resize() != RESIZE_NONE)
        return PaintInvalidationBorderBoxChange;

    if (style()->hasBorderRadius()) {
        // If a border-radius exists and width/height is smaller than radius width/height,
        // we need to fully invalidate to cover the changed radius.
        FloatRoundedRect oldRoundedRect = style()->getRoundedBorderFor(LayoutRect(LayoutPoint(0, 0), oldBorderBoxSize));
        FloatRoundedRect newRoundedRect = style()->getRoundedBorderFor(LayoutRect(LayoutPoint(0, 0), newBorderBoxSize));
        if (oldRoundedRect.getRadii() != newRoundedRect.getRadii())
            return PaintInvalidationBorderBoxChange;
    }

    if (oldBorderBoxSize.width() != newBorderBoxSize.width() && mustInvalidateBackgroundOrBorderPaintOnWidthChange())
        return PaintInvalidationBorderBoxChange;
    if (oldBorderBoxSize.height() != newBorderBoxSize.height() && mustInvalidateBackgroundOrBorderPaintOnHeightChange())
        return PaintInvalidationBorderBoxChange;

    return PaintInvalidationIncremental;
}

void LayoutBox::incrementallyInvalidatePaint(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect& oldBounds, const LayoutRect& newBounds, const LayoutPoint& positionFromPaintInvalidationBacking)
{
    LayoutObject::incrementallyInvalidatePaint(paintInvalidationContainer, oldBounds, newBounds, positionFromPaintInvalidationBacking);

    bool hasBoxDecorations = style()->hasBoxDecorations();
    if (!style()->hasBackground() && !hasBoxDecorations)
        return;

    LayoutSize oldBorderBoxSize = computePreviousBorderBoxSize(oldBounds.size());
    LayoutSize newBorderBoxSize = size();

    // If border box size didn't change, LayoutObject's incrementallyInvalidatePaint() is good.
    if (oldBorderBoxSize == newBorderBoxSize)
        return;

    // If size of the paint invalidation rect equals to size of border box, LayoutObject::incrementallyInvalidatePaint()
    // is good for boxes having background without box decorations.
    ASSERT(oldBounds.location() == newBounds.location()); // Otherwise we won't do incremental invalidation.
    if (!hasBoxDecorations
        && positionFromPaintInvalidationBacking == newBounds.location()
        && oldBorderBoxSize == oldBounds.size()
        && newBorderBoxSize == newBounds.size())
        return;

    // Invalidate the right delta part and the right border of the old or new box which has smaller width.
    LayoutUnit deltaWidth = absoluteValue(oldBorderBoxSize.width() - newBorderBoxSize.width());
    if (deltaWidth) {
        LayoutUnit smallerWidth = std::min(oldBorderBoxSize.width(), newBorderBoxSize.width());
        LayoutUnit borderTopRightRadiusWidth = valueForLength(style()->borderTopRightRadius().width(), smallerWidth);
        LayoutUnit borderBottomRightRadiusWidth = valueForLength(style()->borderBottomRightRadius().width(), smallerWidth);
        LayoutUnit borderWidth = std::max(LayoutUnit(borderRight()), std::max(borderTopRightRadiusWidth, borderBottomRightRadiusWidth));
        LayoutRect rightDeltaRect(positionFromPaintInvalidationBacking.x() + smallerWidth - borderWidth,
            positionFromPaintInvalidationBacking.y(),
            deltaWidth + borderWidth,
            std::max(oldBorderBoxSize.height(), newBorderBoxSize.height()));
        invalidatePaintRectClippedByOldAndNewBounds(paintInvalidationContainer, rightDeltaRect, oldBounds, newBounds);
    }

    // Invalidate the bottom delta part and the bottom border of the old or new box which has smaller height.
    LayoutUnit deltaHeight = absoluteValue(oldBorderBoxSize.height() - newBorderBoxSize.height());
    if (deltaHeight) {
        LayoutUnit smallerHeight = std::min(oldBorderBoxSize.height(), newBorderBoxSize.height());
        LayoutUnit borderBottomLeftRadiusHeight = valueForLength(style()->borderBottomLeftRadius().height(), smallerHeight);
        LayoutUnit borderBottomRightRadiusHeight = valueForLength(style()->borderBottomRightRadius().height(), smallerHeight);
        LayoutUnit borderHeight = std::max(LayoutUnit(borderBottom()), std::max(borderBottomLeftRadiusHeight, borderBottomRightRadiusHeight));
        LayoutRect bottomDeltaRect(positionFromPaintInvalidationBacking.x(),
            positionFromPaintInvalidationBacking.y() + smallerHeight - borderHeight,
            std::max(oldBorderBoxSize.width(), newBorderBoxSize.width()),
            deltaHeight + borderHeight);
        invalidatePaintRectClippedByOldAndNewBounds(paintInvalidationContainer, bottomDeltaRect, oldBounds, newBounds);
    }
}

void LayoutBox::invalidatePaintRectClippedByOldAndNewBounds(const LayoutBoxModelObject& paintInvalidationContainer, const LayoutRect& rect, const LayoutRect& oldBounds, const LayoutRect& newBounds)
{
    if (rect.isEmpty())
        return;
    LayoutRect rectClippedByOldBounds = intersection(rect, oldBounds);
    LayoutRect rectClippedByNewBounds = intersection(rect, newBounds);
    // Invalidate only once if the clipped rects equal.
    if (rectClippedByOldBounds == rectClippedByNewBounds) {
        invalidatePaintUsingContainer(paintInvalidationContainer, rectClippedByOldBounds, PaintInvalidationIncremental);
        return;
    }
    // Invalidate the bigger one if one contains another. Otherwise invalidate both.
    if (!rectClippedByNewBounds.contains(rectClippedByOldBounds))
        invalidatePaintUsingContainer(paintInvalidationContainer, rectClippedByOldBounds, PaintInvalidationIncremental);
    if (!rectClippedByOldBounds.contains(rectClippedByNewBounds))
        invalidatePaintUsingContainer(paintInvalidationContainer, rectClippedByNewBounds, PaintInvalidationIncremental);
}

void LayoutBox::markForPaginationRelayoutIfNeeded(SubtreeLayoutScope& layoutScope)
{
    ASSERT(!needsLayout());
    // If fragmentation height has changed, we need to lay out. No need to enter the layoutObject if it
    // is childless, though.
    if (view()->layoutState()->pageLogicalHeightChanged() && slowFirstChild())
        layoutScope.setChildNeedsLayout(this);
}

void LayoutBox::markOrthogonalWritingModeRoot()
{
    ASSERT(frameView());
    frameView()->addOrthogonalWritingModeRoot(*this);
}

void LayoutBox::unmarkOrthogonalWritingModeRoot()
{
    ASSERT(frameView());
    frameView()->removeOrthogonalWritingModeRoot(*this);
}

void LayoutBox::addVisualEffectOverflow()
{
    if (!style()->hasVisualOverflowingEffect())
        return;

    // Add in the final overflow with shadows, outsets and outline combined.
    LayoutRect visualEffectOverflow = borderBoxRect();
    visualEffectOverflow.expand(computeVisualEffectOverflowOutsets());
    addSelfVisualOverflow(visualEffectOverflow);
}

LayoutRectOutsets LayoutBox::computeVisualEffectOverflowOutsets() const
{
    ASSERT(style()->hasVisualOverflowingEffect());

    LayoutUnit top;
    LayoutUnit right;
    LayoutUnit bottom;
    LayoutUnit left;

    if (const ShadowList* boxShadow = style()->boxShadow()) {
        // FIXME: Use LayoutUnit edge outsets, and then simplify this.
        FloatRectOutsets outsets = boxShadow->rectOutsetsIncludingOriginal();
        top = LayoutUnit(outsets.top());
        right = LayoutUnit(outsets.right());
        bottom = LayoutUnit(outsets.bottom());
        left = LayoutUnit(outsets.left());
    }

    if (style()->hasBorderImageOutsets()) {
        LayoutRectOutsets borderOutsets = style()->borderImageOutsets();
        top = std::max(top, borderOutsets.top());
        right = std::max(right, borderOutsets.right());
        bottom = std::max(bottom, borderOutsets.bottom());
        left = std::max(left, borderOutsets.left());
    }

    // Box-shadow and border-image-outsets are in physical direction. Flip into block direction.
    if (UNLIKELY(hasFlippedBlocksWritingMode()))
        std::swap(left, right);

    if (style()->hasOutline()) {
        Vector<LayoutRect> outlineRects;
        // The result rects are in coordinates of this object's border box.
        addOutlineRects(outlineRects, LayoutPoint(), outlineRectsShouldIncludeBlockVisualOverflow());
        LayoutRect rect = unionRectEvenIfEmpty(outlineRects);
        int outlineOutset = style()->outlineOutsetExtent();
        top = std::max(top, -rect.y() + outlineOutset);
        right = std::max(right, rect.maxX() - size().width() + outlineOutset);
        bottom = std::max(bottom, rect.maxY() - size().height() + outlineOutset);
        left = std::max(left, -rect.x() + outlineOutset);
    }

    return LayoutRectOutsets(top, right, bottom, left);
}

void LayoutBox::addOverflowFromChild(LayoutBox* child, const LayoutSize& delta)
{
    // Never allow flow threads to propagate overflow up to a parent.
    if (child->isLayoutFlowThread())
        return;

    // Only propagate layout overflow from the child if the child isn't clipping its overflow.  If it is, then
    // its overflow is internal to it, and we don't care about it.  layoutOverflowRectForPropagation takes care of this
    // and just propagates the border box rect instead.
    LayoutRect childLayoutOverflowRect = child->layoutOverflowRectForPropagation(styleRef());
    childLayoutOverflowRect.move(delta);
    addLayoutOverflow(childLayoutOverflowRect);

    // Add in visual overflow from the child.  Even if the child clips its overflow, it may still
    // have visual overflow of its own set from box shadows or reflections.  It is unnecessary to propagate this
    // overflow if we are clipping our own overflow.
    if (child->hasSelfPaintingLayer())
        return;
    LayoutRect childVisualOverflowRect = child->visualOverflowRectForPropagation(styleRef());
    childVisualOverflowRect.move(delta);
    addContentsVisualOverflow(childVisualOverflowRect);
}

bool LayoutBox::hasTopOverflow() const
{
    return !style()->isLeftToRightDirection() && !isHorizontalWritingMode();
}

bool LayoutBox::hasLeftOverflow() const
{
    return !style()->isLeftToRightDirection() && isHorizontalWritingMode();
}

void LayoutBox::addLayoutOverflow(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    LayoutRect clientBox = noOverflowRect();
    if (clientBox.contains(rect))
        return;

    // For overflow clip objects, we don't want to propagate overflow into unreachable areas.
    LayoutRect overflowRect(rect);
    if (hasOverflowClip() || isLayoutView()) {
        // Overflow is in the block's coordinate space and thus is flipped for vertical-rl writing
        // mode.  At this stage that is actually a simplification, since we can treat vertical-lr/rl
        // as the same.
        if (hasTopOverflow())
            overflowRect.shiftMaxYEdgeTo(std::min(overflowRect.maxY(), clientBox.maxY()));
        else
            overflowRect.shiftYEdgeTo(std::max(overflowRect.y(), clientBox.y()));
        if (hasLeftOverflow())
            overflowRect.shiftMaxXEdgeTo(std::min(overflowRect.maxX(), clientBox.maxX()));
        else
            overflowRect.shiftXEdgeTo(std::max(overflowRect.x(), clientBox.x()));

        // Now re-test with the adjusted rectangle and see if it has become unreachable or fully
        // contained.
        if (clientBox.contains(overflowRect) || overflowRect.isEmpty())
            return;
    }

    if (!m_overflow)
        m_overflow = wrapUnique(new BoxOverflowModel(clientBox, borderBoxRect()));

    m_overflow->addLayoutOverflow(overflowRect);
}

void LayoutBox::addSelfVisualOverflow(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    LayoutRect borderBox = borderBoxRect();
    if (borderBox.contains(rect))
        return;

    if (!m_overflow)
        m_overflow = wrapUnique(new BoxOverflowModel(noOverflowRect(), borderBox));

    m_overflow->addSelfVisualOverflow(rect);
}

void LayoutBox::addContentsVisualOverflow(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;

    // If hasOverflowClip() we always save contents visual overflow because we need it
    // e.g. to determine whether to apply rounded corner clip on contents.
    // Otherwise we save contents visual overflow only if it overflows the border box.
    LayoutRect borderBox = borderBoxRect();
    if (!hasOverflowClip() && borderBox.contains(rect))
        return;

    if (!m_overflow)
        m_overflow = wrapUnique(new BoxOverflowModel(noOverflowRect(), borderBox));
    m_overflow->addContentsVisualOverflow(rect);
}

void LayoutBox::clearLayoutOverflow()
{
    if (!m_overflow)
        return;

    if (!hasSelfVisualOverflow() && contentsVisualOverflowRect().isEmpty()) {
        clearAllOverflows();
        return;
    }

    m_overflow->setLayoutOverflow(noOverflowRect());
}

bool LayoutBox::percentageLogicalHeightIsResolvable() const
{
    Length fakeLength(100, Percent);
    return computePercentageLogicalHeight(fakeLength) != -1;
}

bool LayoutBox::hasUnsplittableScrollingOverflow() const
{
    // We will paginate as long as we don't scroll overflow in the pagination direction.
    bool isHorizontal = isHorizontalWritingMode();
    if ((isHorizontal && !scrollsOverflowY()) || (!isHorizontal && !scrollsOverflowX()))
        return false;

    // We do have overflow. We'll still be willing to paginate as long as the block
    // has auto logical height, auto or undefined max-logical-height and a zero or auto min-logical-height.
    // Note this is just a heuristic, and it's still possible to have overflow under these
    // conditions, but it should work out to be good enough for common cases. Paginating overflow
    // with scrollbars present is not the end of the world and is what we used to do in the old model anyway.
    return !style()->logicalHeight().isIntrinsicOrAuto()
        || (!style()->logicalMaxHeight().isIntrinsicOrAuto() && !style()->logicalMaxHeight().isMaxSizeNone() && (!style()->logicalMaxHeight().hasPercent() || percentageLogicalHeightIsResolvable()))
        || (!style()->logicalMinHeight().isIntrinsicOrAuto() && style()->logicalMinHeight().isPositive() && (!style()->logicalMinHeight().hasPercent() || percentageLogicalHeightIsResolvable()));
}

LayoutBox::PaginationBreakability LayoutBox::getPaginationBreakability() const
{
    // TODO(mstensho): It is wrong to check isAtomicInlineLevel() as we
    // actually look for replaced elements.
    if (isAtomicInlineLevel()
        || hasUnsplittableScrollingOverflow()
        || (parent() && isWritingModeRoot())
        || (isOutOfFlowPositioned() && style()->position() == FixedPosition))
        return ForbidBreaks;

    EBreak breakValue = breakInside();
    if (breakValue == BreakAvoid || breakValue == BreakAvoidPage || breakValue == BreakAvoidColumn)
        return AvoidBreaks;
    return AllowAnyBreaks;
}

LayoutUnit LayoutBox::lineHeight(bool /*firstLine*/, LineDirectionMode direction, LinePositionMode /*linePositionMode*/) const
{
    if (isAtomicInlineLevel())
        return direction == HorizontalLine ? marginHeight() + size().height() : marginWidth() + size().width();
    return LayoutUnit();
}

int LayoutBox::baselinePosition(FontBaseline baselineType, bool /*firstLine*/, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    ASSERT(linePositionMode == PositionOnContainingLine);
    if (isAtomicInlineLevel()) {
        int result = direction == HorizontalLine ? roundToInt(marginHeight() + size().height()) : roundToInt(marginWidth() + size().width());
        if (baselineType == AlphabeticBaseline)
            return result;
        return result - result / 2;
    }
    return 0;
}


PaintLayer* LayoutBox::enclosingFloatPaintingLayer() const
{
    const LayoutObject* curr = this;
    while (curr) {
        PaintLayer* layer = curr->hasLayer() && curr->isBox() ? toLayoutBox(curr)->layer() : 0;
        if (layer && layer->isSelfPaintingLayer())
            return layer;
        curr = curr->parent();
    }
    return nullptr;
}

LayoutRect LayoutBox::logicalVisualOverflowRectForPropagation(const ComputedStyle& parentStyle) const
{
    LayoutRect rect = visualOverflowRectForPropagation(parentStyle);
    if (!parentStyle.isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect LayoutBox::visualOverflowRectForPropagation(const ComputedStyle& parentStyle) const
{
    // If the writing modes of the child and parent match, then we don't have to
    // do anything fancy. Just return the result.
    LayoutRect rect = visualOverflowRect();
    if (parentStyle.getWritingMode() == style()->getWritingMode())
        return rect;

    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style()->getWritingMode() == RightToLeftWritingMode || parentStyle.getWritingMode() == RightToLeftWritingMode)
        rect.setX(size().width() - rect.maxX());

    return rect;
}

LayoutRect LayoutBox::logicalLayoutOverflowRectForPropagation(const ComputedStyle& parentStyle) const
{
    LayoutRect rect = layoutOverflowRectForPropagation(parentStyle);
    if (!parentStyle.isHorizontalWritingMode())
        return rect.transposedRect();
    return rect;
}

LayoutRect LayoutBox::layoutOverflowRectForPropagation(const ComputedStyle& parentStyle) const
{
    // Only propagate interior layout overflow if we don't clip it.
    LayoutRect rect = borderBoxRect();
    // We want to include the margin, but only when it adds height. Quirky margins don't contribute height
    // nor do the margins of self-collapsing blocks.
    if (!styleRef().hasMarginAfterQuirk() && !isSelfCollapsingBlock())
        rect.expand(isHorizontalWritingMode() ? LayoutSize(LayoutUnit(), marginAfter()) : LayoutSize(marginAfter(), LayoutUnit()));

    if (!hasOverflowClip())
        rect.unite(layoutOverflowRect());

    bool hasTransform = hasLayer() && layer()->transform();
    if (isInFlowPositioned() || hasTransform) {
        // If we are relatively positioned or if we have a transform, then we have to convert
        // this rectangle into physical coordinates, apply relative positioning and transforms
        // to it, and then convert it back.
        flipForWritingMode(rect);

        if (hasTransform)
            rect = layer()->currentTransform().mapRect(rect);

        if (isInFlowPositioned())
            rect.move(offsetForInFlowPosition());

        // Now we need to flip back.
        flipForWritingMode(rect);
    }

    // If the writing modes of the child and parent match, then we don't have to
    // do anything fancy. Just return the result.
    if (parentStyle.getWritingMode() == style()->getWritingMode())
        return rect;

    // We are putting ourselves into our parent's coordinate space.  If there is a flipped block mismatch
    // in a particular axis, then we have to flip the rect along that axis.
    if (style()->getWritingMode() == RightToLeftWritingMode || parentStyle.getWritingMode() == RightToLeftWritingMode)
        rect.setX(size().width() - rect.maxX());

    return rect;
}

LayoutRect LayoutBox::noOverflowRect() const
{
    // Because of the special coordinate system used for overflow rectangles and many other
    // rectangles (not quite logical, not quite physical), we need to flip the block progression
    // coordinate in vertical-rl writing mode. In other words, the rectangle returned is physical,
    // except for the block direction progression coordinate (x in vertical writing mode), which is
    // always "logical top". Apart from the flipping, this method does the same thing as
    // clientBoxRect().

    const int scrollBarWidth = verticalScrollbarWidth();
    const int scrollBarHeight = horizontalScrollbarHeight();
    LayoutUnit left(borderLeft() + (shouldPlaceBlockDirectionScrollbarOnLogicalLeft() ? scrollBarWidth : 0));
    LayoutUnit top(borderTop());
    LayoutUnit right(borderRight());
    LayoutUnit bottom(borderBottom());
    LayoutRect rect(left, top, size().width() - left - right, size().height() - top - bottom);
    flipForWritingMode(rect);
    // Subtract space occupied by scrollbars. Order is important here: first flip, then subtract
    // scrollbars. This may seem backwards and weird, since one would think that a vertical
    // scrollbar at the physical right in vertical-rl ought to be at the logical left (physical
    // right), between the logical left (physical right) border and the logical left (physical
    // right) padding. But this is how the rest of the code expects us to behave. This is highly
    // related to https://bugs.webkit.org/show_bug.cgi?id=76129
    // FIXME: when the above mentioned bug is fixed, it should hopefully be possible to call
    // clientBoxRect() or paddingBoxRect() in this method, rather than fiddling with the edges on
    // our own.
    if (shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        rect.contract(0, scrollBarHeight);
    else
        rect.contract(scrollBarWidth, scrollBarHeight);
    return rect;
}

LayoutRect LayoutBox::visualOverflowRect() const
{
    if (!m_overflow)
        return borderBoxRect();
    if (hasOverflowClip())
        return m_overflow->selfVisualOverflowRect();
    return unionRect(m_overflow->selfVisualOverflowRect(), m_overflow->contentsVisualOverflowRect());
}

LayoutUnit LayoutBox::offsetLeft(const Element* parent) const
{
    return adjustedPositionRelativeTo(topLeftLocation(), parent).x();
}

LayoutUnit LayoutBox::offsetTop(const Element* parent) const
{
    return adjustedPositionRelativeTo(topLeftLocation(), parent).y();
}

LayoutPoint LayoutBox::flipForWritingModeForChild(const LayoutBox* child, const LayoutPoint& point) const
{
    if (!style()->isFlippedBlocksWritingMode())
        return point;

    // The child is going to add in its x(), so we have to make sure it ends up in
    // the right place.
    return LayoutPoint(point.x() + size().width() - child->size().width() - (2 * child->location().x()), point.y());
}

LayoutPoint LayoutBox::topLeftLocation() const
{
    LayoutBlock* containerBlock = containingBlock();
    if (!containerBlock || containerBlock == this)
        return location();
    return containerBlock->flipForWritingModeForChild(this, location());
}

bool LayoutBox::hasRelativeLogicalWidth() const
{
    return style()->logicalWidth().hasPercent()
        || style()->logicalMinWidth().hasPercent()
        || style()->logicalMaxWidth().hasPercent();
}

bool LayoutBox::hasRelativeLogicalHeight() const
{
    return style()->logicalHeight().hasPercent()
        || style()->logicalMinHeight().hasPercent()
        || style()->logicalMaxHeight().hasPercent();
}

static void markBoxForRelayoutAfterSplit(LayoutBox* box)
{
    // FIXME: The table code should handle that automatically. If not,
    // we should fix it and remove the table part checks.
    if (box->isTable()) {
        // Because we may have added some sections with already computed column structures, we need to
        // sync the table structure with them now. This avoids crashes when adding new cells to the table.
        toLayoutTable(box)->forceSectionsRecalc();
    } else if (box->isTableSection()) {
        toLayoutTableSection(box)->setNeedsCellRecalc();
    }

    box->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::AnonymousBlockChange);
}

static void collapseLoneAnonymousBlockChild(LayoutBox* parent, LayoutObject* child)
{
    if (!child->isAnonymousBlock() || !child->isLayoutBlockFlow())
        return;
    if (!parent->isLayoutBlockFlow())
        return;
    toLayoutBlockFlow(parent)->collapseAnonymousBlockChild(toLayoutBlockFlow(child));
}

LayoutObject* LayoutBox::splitAnonymousBoxesAroundChild(LayoutObject* beforeChild)
{
    LayoutBox* boxAtTopOfNewBranch = nullptr;

    while (beforeChild->parent() != this) {
        LayoutBox* boxToSplit = toLayoutBox(beforeChild->parent());
        if (boxToSplit->slowFirstChild() != beforeChild && boxToSplit->isAnonymous()) {

            // We have to split the parent box into two boxes and move children
            // from |beforeChild| to end into the new post box.
            LayoutBox* postBox = boxToSplit->createAnonymousBoxWithSameTypeAs(this);
            postBox->setChildrenInline(boxToSplit->childrenInline());
            LayoutBox* parentBox = toLayoutBox(boxToSplit->parent());
            // We need to invalidate the |parentBox| before inserting the new node
            // so that the table paint invalidation logic knows the structure is dirty.
            // See for example LayoutTableCell:localOverflowRectForPaintInvalidation.
            markBoxForRelayoutAfterSplit(parentBox);
            parentBox->virtualChildren()->insertChildNode(parentBox, postBox, boxToSplit->nextSibling());
            boxToSplit->moveChildrenTo(postBox, beforeChild, 0, true);

            LayoutObject* child = postBox->slowFirstChild();
            ASSERT(child);
            if (child && !child->nextSibling())
                collapseLoneAnonymousBlockChild(postBox, child);
            child = boxToSplit->slowFirstChild();
            ASSERT(child);
            if (child && !child->nextSibling())
                collapseLoneAnonymousBlockChild(boxToSplit, child);

            markBoxForRelayoutAfterSplit(boxToSplit);
            markBoxForRelayoutAfterSplit(postBox);
            boxAtTopOfNewBranch = postBox;

            beforeChild = postBox;
        } else {
            beforeChild = boxToSplit;
        }
    }

    // Splitting the box means the left side of the container chain will lose any percent height descendants
    // below |boxAtTopOfNewBranch| on the right hand side.
    if (boxAtTopOfNewBranch) {
        boxAtTopOfNewBranch->clearPercentHeightDescendants();
        markBoxForRelayoutAfterSplit(this);
    }

    ASSERT(beforeChild->parent() == this);
    return beforeChild;
}

LayoutUnit LayoutBox::offsetFromLogicalTopOfFirstPage() const
{
    LayoutState* layoutState = view()->layoutState();
    if (!layoutState || !layoutState->isPaginated())
        return LayoutUnit();

    if (layoutState->layoutObject() == this) {
        LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
        return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }

    // A LayoutBlock always establishes a layout state, and this method is only meant to be called
    // on the object currently being laid out.
    ASSERT(!isLayoutBlock());

    // In case this box doesn't establish a layout state, try the containing block.
    LayoutBlock* containerBlock = containingBlock();
    ASSERT(layoutState->layoutObject() == containerBlock);
    return containerBlock->offsetFromLogicalTopOfFirstPage() + logicalTop();
}

void LayoutBox::setPageLogicalOffset(LayoutUnit offset)
{
    if (!m_rareData && !offset)
        return;
    ensureRareData().m_pageLogicalOffset = offset;
}

bool LayoutBox::needToSavePreviousBoxSizes()
{
    // If m_rareData is already created, always save.
    if (m_rareData)
        return true;

    LayoutSize paintInvalidationSize = previousPaintInvalidationRectSize();
    // Don't save old box sizes if the paint rect is empty because we'll
    // full invalidate once the paint rect becomes non-empty.
    if (paintInvalidationSize.isEmpty())
        return false;

    // If we use border-box sizing we need to track changes in the size of the content box.
    if (style()->boxSizing() == BoxSizingBorderBox)
        return true;

    // We need the old box sizes only when the box has background, decorations, or masks.
    // Main LayoutView paints base background, thus interested in box size.
    if (!isLayoutView() && !style()->hasBackground() && !style()->hasBoxDecorations() && !style()->hasMask())
        return false;

    // No need to save old border box size if we can use size of the old paint
    // rect as the old border box size in the next invalidation.
    if (paintInvalidationSize != size())
        return true;

    // Background and mask layers can depend on other boxes than border box. See crbug.com/490533
    if (style()->backgroundLayers().thisOrNextLayersUseContentBox() || style()->backgroundLayers().thisOrNextLayersHaveLocalAttachment()
        || style()->maskLayers().thisOrNextLayersUseContentBox())
        return true;

    return false;
}

void LayoutBox::savePreviousBoxSizesIfNeeded()
{
    if (!needToSavePreviousBoxSizes())
        return;

    LayoutBoxRareData& rareData = ensureRareData();
    rareData.m_previousBorderBoxSize = size();
    rareData.m_previousContentBoxRect = contentBoxRect();
    rareData.m_previousLayoutOverflowRect = layoutOverflowRect();
}

LayoutSize LayoutBox::computePreviousBorderBoxSize(const LayoutSize& previousBoundsSize) const
{
    // PreviousBorderBoxSize is only valid when there is background or box decorations.
    ASSERT(style()->hasBackground() || style()->hasBoxDecorations());

    if (m_rareData && m_rareData->m_previousBorderBoxSize.width() != -1)
        return m_rareData->m_previousBorderBoxSize;

    // We didn't save the old border box size because it was the same as the size of oldBounds.
    return previousBoundsSize;
}

void LayoutBox::logicalExtentAfterUpdatingLogicalWidth(const LayoutUnit& newLogicalTop, LayoutBox::LogicalExtentComputedValues& computedValues)
{
    // FIXME: None of this is right for perpendicular writing-mode children.
    LayoutUnit oldLogicalWidth = logicalWidth();
    LayoutUnit oldLogicalLeft = logicalLeft();
    LayoutUnit oldMarginLeft = marginLeft();
    LayoutUnit oldMarginRight = marginRight();
    LayoutUnit oldLogicalTop = logicalTop();

    setLogicalTop(newLogicalTop);
    updateLogicalWidth();

    computedValues.m_extent = logicalWidth();
    computedValues.m_position = logicalLeft();
    computedValues.m_margins.m_start = marginStart();
    computedValues.m_margins.m_end = marginEnd();

    setLogicalTop(oldLogicalTop);
    setLogicalWidth(oldLogicalWidth);
    setLogicalLeft(oldLogicalLeft);
    setMarginLeft(oldMarginLeft);
    setMarginRight(oldMarginRight);
}

bool LayoutBox::mustInvalidateFillLayersPaintOnHeightChange(const FillLayer& layer) const
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer.next())
        return true;

    // Make sure we have a valid image.
    StyleImage* img = layer.image();
    if (!img || !img->canRender())
        return false;

    if (layer.repeatY() != RepeatFill && layer.repeatY() != NoRepeatFill)
        return true;

    // TODO(alancutter): Make this work correctly for calc lengths.
    if (layer.yPosition().hasPercent() && !layer.yPosition().isZero())
        return true;

    if (layer.backgroundYOrigin() != TopEdge)
        return true;

    EFillSizeType sizeType = layer.sizeType();

    if (sizeType == Contain || sizeType == Cover)
        return true;

    if (sizeType == SizeLength) {
        // TODO(alancutter): Make this work correctly for calc lengths.
        if (layer.sizeLength().height().hasPercent() && !layer.sizeLength().height().isZero())
            return true;
        if (img->isGeneratedImage() && layer.sizeLength().height().isAuto())
            return true;
    } else if (img->usesImageContainerSize()) {
        return true;
    }

    return false;
}

bool LayoutBox::mustInvalidateFillLayersPaintOnWidthChange(const FillLayer& layer) const
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer.next())
        return true;

    // Make sure we have a valid image.
    StyleImage* img = layer.image();
    if (!img || !img->canRender())
        return false;

    if (layer.repeatX() != RepeatFill && layer.repeatX() != NoRepeatFill)
        return true;

    // TODO(alancutter): Make this work correctly for calc lengths.
    if (layer.xPosition().hasPercent() && !layer.xPosition().isZero())
        return true;

    if (layer.backgroundXOrigin() != LeftEdge)
        return true;

    EFillSizeType sizeType = layer.sizeType();

    if (sizeType == Contain || sizeType == Cover)
        return true;

    if (sizeType == SizeLength) {
        // TODO(alancutter): Make this work correctly for calc lengths.
        if (layer.sizeLength().width().hasPercent() && !layer.sizeLength().width().isZero())
            return true;
        if (img->isGeneratedImage() && layer.sizeLength().width().isAuto())
            return true;
    } else if (img->usesImageContainerSize()) {
        return true;
    }

    return false;
}

bool LayoutBox::mustInvalidateBackgroundOrBorderPaintOnWidthChange() const
{
    if (hasMask() && mustInvalidateFillLayersPaintOnWidthChange(style()->maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!hasBoxDecorationBackground())
        return false;

    if (mustInvalidateFillLayersPaintOnWidthChange(style()->backgroundLayers()))
        return true;

    // Our fill layers are ok. Let's check border.
    if (style()->hasBorderDecoration() && canRenderBorderImage())
        return true;

    return false;
}

bool LayoutBox::mustInvalidateBackgroundOrBorderPaintOnHeightChange() const
{
    if (hasMask() && mustInvalidateFillLayersPaintOnHeightChange(style()->maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!hasBoxDecorationBackground())
        return false;

    if (mustInvalidateFillLayersPaintOnHeightChange(style()->backgroundLayers()))
        return true;

    // Our fill layers are ok.  Let's check border.
    if (style()->hasBorderDecoration() && canRenderBorderImage())
        return true;

    return false;
}

bool LayoutBox::canRenderBorderImage() const
{
    if (!style()->hasBorderDecoration())
        return false;

    StyleImage* borderImage = style()->borderImage().image();
    return borderImage && borderImage->canRender() && borderImage->isLoaded();
}

ShapeOutsideInfo* LayoutBox::shapeOutsideInfo() const
{
    return ShapeOutsideInfo::isEnabledFor(*this) ? ShapeOutsideInfo::info(*this) : nullptr;
}

void LayoutBox::clearPreviousPaintInvalidationRects()
{
    LayoutBoxModelObject::clearPreviousPaintInvalidationRects();
    if (PaintLayerScrollableArea* scrollableArea = this->getScrollableArea())
        scrollableArea->clearPreviousPaintInvalidationRects();
}

void LayoutBox::setPercentHeightContainer(LayoutBlock* container)
{
    ASSERT(!container || !percentHeightContainer());
    if (!container && !m_rareData)
        return;
    ensureRareData().m_percentHeightContainer = container;
}

void LayoutBox::removeFromPercentHeightContainer()
{
    if (!percentHeightContainer())
        return;

    ASSERT(percentHeightContainer()->hasPercentHeightDescendant(this));
    percentHeightContainer()->removePercentHeightDescendant(this);
    // The above call should call this object's setPercentHeightContainer(nullptr).
    ASSERT(!percentHeightContainer());
}

void LayoutBox::clearPercentHeightDescendants()
{
    for (LayoutObject* curr = slowFirstChild(); curr; curr = curr->nextInPreOrder(this)) {
        if (curr->isBox())
            toLayoutBox(curr)->removeFromPercentHeightContainer();
    }
}

LayoutUnit LayoutBox::pageLogicalHeightForOffset(LayoutUnit offset) const
{
    LayoutView* layoutView = view();
    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread)
        return layoutView->pageLogicalHeight();
    return flowThread->pageLogicalHeightForOffset(offset + offsetFromLogicalTopOfFirstPage());
}

LayoutUnit LayoutBox::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule) const
{
    LayoutView* layoutView = view();
    offset += offsetFromLogicalTopOfFirstPage();

    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        LayoutUnit pageLogicalHeight = layoutView->pageLogicalHeight();
        LayoutUnit remainingHeight = pageLogicalHeight - intMod(offset, pageLogicalHeight);
        if (pageBoundaryRule == AssociateWithFormerPage) {
            // An offset exactly at a page boundary will act as being part of the former page in
            // question (i.e. no remaining space), rather than being part of the latter (i.e. one
            // whole page length of remaining space).
            remainingHeight = intMod(remainingHeight, pageLogicalHeight);
        }
        return remainingHeight;
    }

    return flowThread->pageRemainingLogicalHeightForOffset(offset, pageBoundaryRule);
}

LayoutUnit LayoutBox::calculatePaginationStrutToFitContent(LayoutUnit offset, LayoutUnit strutToNextPage, LayoutUnit contentLogicalHeight) const
{
    ASSERT(strutToNextPage == pageRemainingLogicalHeightForOffset(offset, AssociateWithLatterPage));
    LayoutUnit nextPageLogicalTop = offset + strutToNextPage;
    if (pageLogicalHeightForOffset(nextPageLogicalTop) >= contentLogicalHeight)
        return strutToNextPage; // Content fits just fine in the next page or column.

    // Moving to the top of the next page or column doesn't result in enough space for the content
    // that we're trying to fit. If we're in a nested fragmentation context, we may find enough
    // space if we move to a column further ahead, by effectively breaking to the next outer
    // fragmentainer.
    LayoutFlowThread* flowThread = flowThreadContainingBlock();
    if (!flowThread) {
        // If there's no flow thread, we're not nested. All pages have the same height. Give up.
        return strutToNextPage;
    }
    // Start searching for a suitable offset at the top of the next page or column.
    LayoutUnit flowThreadOffset = offsetFromLogicalTopOfFirstPage() + nextPageLogicalTop;
    return strutToNextPage + flowThread->nextLogicalTopForUnbreakableContent(flowThreadOffset, contentLogicalHeight) - flowThreadOffset;
}

LayoutBox* LayoutBox::snapContainer() const
{
    return m_rareData ? m_rareData->m_snapContainer : nullptr;
}

void LayoutBox::setSnapContainer(LayoutBox* newContainer)
{
    LayoutBox* oldContainer = snapContainer();
    if (oldContainer == newContainer)
        return;

    if (oldContainer)
        oldContainer->removeSnapArea(*this);

    ensureRareData().m_snapContainer = newContainer;

    if (newContainer)
        newContainer->addSnapArea(*this);
}

void LayoutBox::clearSnapAreas()
{
    if (SnapAreaSet* areas = snapAreas()) {
        for (auto& snapArea : *areas)
            snapArea->m_rareData->m_snapContainer = nullptr;
        areas->clear();
    }
}

void LayoutBox::addSnapArea(const LayoutBox& snapArea)
{
    ensureRareData().ensureSnapAreas().add(&snapArea);
}

void LayoutBox::removeSnapArea(const LayoutBox& snapArea)
{
    if (m_rareData && m_rareData->m_snapAreas) {
        m_rareData->m_snapAreas->remove(&snapArea);
    }
}

SnapAreaSet* LayoutBox::snapAreas() const
{
    return m_rareData ? m_rareData->m_snapAreas.get() : nullptr;
}

} // namespace blink
