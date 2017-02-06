/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/layout/LayoutBoxModelObject.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBodyElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "wtf/PtrUtil.h"

namespace blink {

class FloatStateForStyleChange {
public:
    static void setWasFloating(LayoutBoxModelObject* boxModelObject, bool wasFloating)
    {
        s_wasFloating = wasFloating;
        s_boxModelObject = boxModelObject;
    }

    static bool wasFloating(LayoutBoxModelObject* boxModelObject)
    {
        ASSERT(boxModelObject == s_boxModelObject);
        return s_wasFloating;
    }

private:
    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
    static LayoutBoxModelObject* s_boxModelObject;
};

bool FloatStateForStyleChange::s_wasFloating = false;
LayoutBoxModelObject* FloatStateForStyleChange::s_boxModelObject = nullptr;

// The HashMap for storing continuation pointers.
// The continuation chain is a singly linked list. As such, the HashMap's value
// is the next pointer associated with the key.
typedef HashMap<const LayoutBoxModelObject*, LayoutBoxModelObject*> ContinuationMap;
static ContinuationMap* continuationMap = nullptr;

void LayoutBoxModelObject::setSelectionState(SelectionState state)
{
    if (state == SelectionInside && getSelectionState() != SelectionNone)
        return;

    if ((state == SelectionStart && getSelectionState() == SelectionEnd)
        || (state == SelectionEnd && getSelectionState() == SelectionStart))
        LayoutObject::setSelectionState(SelectionBoth);
    else
        LayoutObject::setSelectionState(state);

    // FIXME: We should consider whether it is OK propagating to ancestor LayoutInlines.
    // This is a workaround for http://webkit.org/b/32123
    // The containing block can be null in case of an orphaned tree.
    LayoutBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isLayoutView())
        containingBlock->setSelectionState(state);
}

void LayoutBoxModelObject::contentChanged(ContentChangeType changeType)
{
    if (!hasLayer())
        return;

    layer()->contentChanged(changeType);
}

bool LayoutBoxModelObject::hasAcceleratedCompositing() const
{
    return view()->compositor()->hasAcceleratedCompositing();
}

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node)
{
}

bool LayoutBoxModelObject::usesCompositedScrolling() const
{
    return hasOverflowClip() && hasLayer() && layer()->getScrollableArea()->usesCompositedScrolling();
}

LayoutBoxModelObject::~LayoutBoxModelObject()
{
    // Our layer should have been destroyed and cleared by now
    ASSERT(!hasLayer());
    ASSERT(!m_layer);
}

void LayoutBoxModelObject::willBeDestroyed()
{
    ImageQualityController::remove(*this);

    // A continuation of this LayoutObject should be destroyed at subclasses.
    ASSERT(!continuation());

    if (isPositioned()) {
        // Don't use this->view() because the document's layoutView has been set to 0 during destruction.
        if (LocalFrame* frame = this->frame()) {
            if (FrameView* frameView = frame->view()) {
                if (style()->hasViewportConstrainedPosition())
                    frameView->removeViewportConstrainedObject(this);
            }
        }
    }

    LayoutObject::willBeDestroyed();

    destroyLayer();
}

void LayoutBoxModelObject::styleWillChange(StyleDifference diff, const ComputedStyle& newStyle)
{
    // This object's layer may cease to be a stacking context, in which case the paint
    // invalidation container of the children may change. Thus we need to invalidate paint
    // eagerly for all such children.
    if (hasLayer()
        && enclosingLayer()->stackingNode()
        && enclosingLayer()->stackingNode()->isStackingContext()
        && newStyle.hasAutoZIndex()) {
        // The following disablers are valid because we need to invalidate based on the current
        // status.
        DisableCompositingQueryAsserts compositingDisabler;
        DisablePaintInvalidationStateAsserts paintDisabler;
        invalidatePaintIncludingNonCompositingDescendants();
    }

    FloatStateForStyleChange::setWasFloating(this, isFloating());

    if (const ComputedStyle* oldStyle = style()) {
        if (hasLayer() && diff.needsPaintInvalidationSubtree()) {
            if (oldStyle->hasAutoClip() != newStyle.hasAutoClip()
                || oldStyle->clip() != newStyle.clip())
                layer()->clipper().clearClipRectsIncludingDescendants();
        }
    }

    LayoutObject::styleWillChange(diff, newStyle);
}

void LayoutBoxModelObject::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    bool hadTransform = hasTransformRelatedProperty();
    bool hadLayer = hasLayer();
    bool layerWasSelfPainting = hadLayer && layer()->isSelfPaintingLayer();
    bool wasFloatingBeforeStyleChanged = FloatStateForStyleChange::wasFloating(this);
    bool wasHorizontalWritingMode = isHorizontalWritingMode();

    LayoutObject::styleDidChange(diff, oldStyle);
    updateFromStyle();

    // When an out-of-flow-positioned element changes its display between block and inline-block,
    // then an incremental layout on the element's containing block lays out the element through
    // LayoutPositionedObjects, which skips laying out the element's parent.
    // The element's parent needs to relayout so that it calls
    // LayoutBlockFlow::setStaticInlinePositionForChild with the out-of-flow-positioned child, so
    // that when it's laid out, its LayoutBox::computePositionedLogicalWidth/Height takes into
    // account its new inline/block position rather than its old block/inline position.
    // Position changes and other types of display changes are handled elsewhere.
    if (oldStyle && isOutOfFlowPositioned() && parent() && (parent() != containingBlock())
        && (styleRef().position() == oldStyle->position())
        && (styleRef().originalDisplay() != oldStyle->originalDisplay())
        && ((styleRef().originalDisplay() == BLOCK) || (styleRef().originalDisplay() == INLINE_BLOCK))
        && ((oldStyle->originalDisplay() == BLOCK) || (oldStyle->originalDisplay() == INLINE_BLOCK)))
        parent()->setNeedsLayout(LayoutInvalidationReason::ChildChanged, MarkContainerChain);

    PaintLayerType type = layerTypeRequired();
    if (type != NoPaintLayer) {
        if (!layer() && layerCreationAllowedForSubtree()) {
            if (wasFloatingBeforeStyleChanged && isFloating())
                setChildNeedsLayout();
            createLayer();
            if (parent() && !needsLayout()) {
                // FIXME: We should call a specialized version of this function.
                layer()->updateLayerPositionsAfterLayout();
            }
        }
    } else if (layer() && layer()->parent()) {
        PaintLayer* parentLayer = layer()->parent();
        setHasTransformRelatedProperty(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        layer()->removeOnlyThisLayerAfterStyleChange(); // calls destroyLayer() which clears m_layer
        if (wasFloatingBeforeStyleChanged && isFloating())
            setChildNeedsLayout();
        if (hadTransform)
            setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(LayoutInvalidationReason::StyleChange);
        if (!needsLayout()) {
            // FIXME: We should call a specialized version of this function.
            parentLayer->updateLayerPositionsAfterLayout();
        }
    }

    if (layer()) {
        layer()->styleDidChange(diff, oldStyle);
        if (hadLayer && layer()->isSelfPaintingLayer() != layerWasSelfPainting)
            setChildNeedsLayout();
    }

    if (oldStyle && wasHorizontalWritingMode != isHorizontalWritingMode()) {
        // Changing the getWritingMode() may change isOrthogonalWritingModeRoot()
        // of children. Make sure all children are marked/unmarked as orthogonal
        // writing-mode roots.
        bool newHorizontalWritingMode = isHorizontalWritingMode();
        for (LayoutObject* child = slowFirstChild(); child; child = child->nextSibling()) {
            if (!child->isBox())
                continue;
            if (newHorizontalWritingMode != child->isHorizontalWritingMode())
                toLayoutBox(child)->markOrthogonalWritingModeRoot();
            else
                toLayoutBox(child)->unmarkOrthogonalWritingModeRoot();
        }
    }

    // Fixed-position is painted using transform. In the case that the object
    // gets the same layout after changing position property, although no
    // re-raster (rect-based invalidation) is needed, display items should
    // still update their paint offset.
    if (oldStyle) {
        bool newStyleIsFixedPosition = style()->position() == FixedPosition;
        bool oldStyleIsFixedPosition = oldStyle->position() == FixedPosition;
        if (newStyleIsFixedPosition != oldStyleIsFixedPosition)
            invalidateDisplayItemClientsIncludingNonCompositingDescendants(PaintInvalidationStyleChange);
    }

    // The used style for body background may change due to computed style change
    // on the document element because of background stealing.
    // Refer to backgroundStolenForBeingBody() and
    // http://www.w3.org/TR/css3-background/#body-background for more info.
    if (isDocumentElement()) {
        HTMLBodyElement* body = document().firstBodyElement();
        LayoutObject* bodyLayout = body ? body->layoutObject() : nullptr;
        if (bodyLayout && bodyLayout->isBoxModelObject()) {
            bool newStoleBodyBackground = toLayoutBoxModelObject(bodyLayout)->backgroundStolenForBeingBody(style());
            bool oldStoleBodyBackground = oldStyle && toLayoutBoxModelObject(bodyLayout)->backgroundStolenForBeingBody(oldStyle);
            if (newStoleBodyBackground != oldStoleBodyBackground
                && bodyLayout->style() && bodyLayout->style()->hasBackground()) {
                bodyLayout->setShouldDoFullPaintInvalidation();
            }
        }
    }

    if (FrameView *frameView = view()->frameView()) {
        bool newStyleIsViewportConstained = style()->position() == FixedPosition;
        bool oldStyleIsViewportConstrained = oldStyle && oldStyle->position() == FixedPosition;
        bool newStyleIsSticky = style()->position() == StickyPosition;
        bool oldStyleIsSticky = oldStyle && oldStyle->position() == StickyPosition;

        if (newStyleIsSticky != oldStyleIsSticky) {
            if (newStyleIsSticky) {
                frameView->addStickyPositionObject();
                // During compositing inputs update we'll have the scroll
                // ancestor without having to walk up the tree and can compute
                // the sticky position constraints then.
                if (layer())
                    layer()->setNeedsCompositingInputsUpdate();
            } else {
                // This may get re-added to viewport constrained objects if the object went
                // from sticky to fixed.
                frameView->removeViewportConstrainedObject(this);
                frameView->removeStickyPositionObject();

                // Remove sticky constraints for this layer.
                if (layer()) {
                    DisableCompositingQueryAsserts disabler;
                    if (const PaintLayer* ancestorOverflowLayer = layer()->ancestorOverflowLayer())
                        ancestorOverflowLayer->getScrollableArea()->invalidateStickyConstraintsFor(layer());
                }
            }
        }

        if (newStyleIsViewportConstained != oldStyleIsViewportConstrained) {
            if (newStyleIsViewportConstained && layer())
                frameView->addViewportConstrainedObject(this);
            else
                frameView->removeViewportConstrainedObject(this);
        }
    }
}

void LayoutBoxModelObject::invalidateStickyConstraints()
{
    if (!layer())
        return;

    // This intentionally uses the stale ancestor overflow layer compositing
    // input as if we have saved constraints for this layer they were saved
    // in the previous frame.
    DisableCompositingQueryAsserts disabler;
    if (const PaintLayer* ancestorOverflowLayer = layer()->ancestorOverflowLayer())
        ancestorOverflowLayer->getScrollableArea()->invalidateAllStickyConstraints();
}

void LayoutBoxModelObject::createLayer()
{
    ASSERT(!m_layer);
    m_layer = wrapUnique(new PaintLayer(this));
    setHasLayer(true);
    m_layer->insertOnlyThisLayerAfterStyleChange();
}

void LayoutBoxModelObject::destroyLayer()
{
    setHasLayer(false);
    m_layer = nullptr;
}

bool LayoutBoxModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

PaintLayerScrollableArea* LayoutBoxModelObject::getScrollableArea() const
{
    return m_layer ? m_layer->getScrollableArea() : 0;
}

void LayoutBoxModelObject::addLayerHitTestRects(LayerHitTestRects& rects, const PaintLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    if (hasLayer()) {
        if (isLayoutView()) {
            // LayoutView is handled with a special fast-path, but it needs to know the current layer.
            LayoutObject::addLayerHitTestRects(rects, layer(), LayoutPoint(), LayoutRect());
        } else {
            // Since a LayoutObject never lives outside it's container Layer, we can switch
            // to marking entire layers instead. This may sometimes mark more than necessary (when
            // a layer is made of disjoint objects) but in practice is a significant performance
            // savings.
            layer()->addLayerHitTestRects(rects);
        }
    } else {
        LayoutObject::addLayerHitTestRects(rects, currentLayer, layerOffset, containerRect);
    }
}

static bool hasPercentageTransform(const ComputedStyle& style)
{
    if (TransformOperation* translate = style.translate()) {
        if (translate->dependsOnBoxSize())
            return true;
    }
    return style.transform().dependsOnBoxSize()
        || (style.transformOriginX() != Length(50, Percent) && style.transformOriginX().hasPercent())
        || (style.transformOriginY() != Length(50, Percent) && style.transformOriginY().hasPercent());
}

void LayoutBoxModelObject::invalidateTreeIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    ASSERT(!needsLayout());

    PaintInvalidationState newPaintInvalidationState(paintInvalidationState, *this);
    if (!shouldCheckForPaintInvalidation(newPaintInvalidationState))
        return;

    if (mayNeedPaintInvalidationSubtree())
        newPaintInvalidationState.setForceSubtreeInvalidationCheckingWithinContainer();

    LayoutRect previousPaintInvalidationRect = this->previousPaintInvalidationRect();
    LayoutPoint previousPosition = previousPositionFromPaintInvalidationBacking();
    PaintInvalidationReason reason = invalidatePaintIfNeeded(newPaintInvalidationState);
    clearPaintInvalidationFlags(newPaintInvalidationState);

    if (previousPosition != previousPositionFromPaintInvalidationBacking())
        newPaintInvalidationState.setForceSubtreeInvalidationCheckingWithinContainer();

    // TODO(wangxianzhu): Combine this function into LayoutObject::invalidateTreeIfNeeded() when removing the following workarounds.

    // TODO(wangxianzhu): This is a workaround for crbug.com/533277. Will remove when we enable paint offset caching.
    if (reason != PaintInvalidationNone && hasPercentageTransform(styleRef()))
        newPaintInvalidationState.setForceSubtreeInvalidationCheckingWithinContainer();

    // TODO(wangxianzhu): This is a workaround for crbug.com/490725. We don't have enough saved information to do accurate check
    // of clipping change. Will remove when we remove rect-based paint invalidation.
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()
        && previousPaintInvalidationRect != this->previousPaintInvalidationRect()
        && !usesCompositedScrolling()
        && hasOverflowClip())
        newPaintInvalidationState.setForceSubtreeInvalidationRectUpdateWithinContainer();

    newPaintInvalidationState.updateForChildren(reason);
    invalidatePaintOfSubtreesIfNeeded(newPaintInvalidationState);
}

void LayoutBoxModelObject::setBackingNeedsPaintInvalidationInRect(const LayoutRect& r, PaintInvalidationReason invalidationReason, const LayoutObject& object) const
{
    // TODO(wangxianzhu): Enable the following assert after paint invalidation for spv2 is ready.
    // ASSERT(!RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(compositingState() != NotComposited);

    // FIXME: generalize accessors to backing GraphicsLayers so that this code is squashing-agnostic.
    if (layer()->groupedMapping()) {
        LayoutRect paintInvalidationRect = r;
        if (GraphicsLayer* squashingLayer = layer()->groupedMapping()->squashingLayer()) {
            // Note: the subpixel accumulation of layer() does not need to be added here. It is already taken into account.
            squashingLayer->setNeedsDisplayInRect(enclosingIntRect(paintInvalidationRect), invalidationReason, object);
        }
    } else if (object.compositedScrollsWithRespectTo(*this)) {
        layer()->compositedLayerMapping()->setScrollingContentsNeedDisplayInRect(r, invalidationReason, object);
    } else if (usesCompositedScrolling()) {
        layer()->compositedLayerMapping()->setNonScrollingContentsNeedDisplayInRect(r, invalidationReason, object);
    } else {
        // Otherwise invalidate everything.
        layer()->compositedLayerMapping()->setContentsNeedDisplayInRect(r, invalidationReason, object);
    }
}

void LayoutBoxModelObject::addOutlineRectsForNormalChildren(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot includeBlockOverflows) const
{
    for (LayoutObject* child = slowFirstChild(); child; child = child->nextSibling()) {
        // Outlines of out-of-flow positioned descendants are handled in LayoutBlock::addOutlineRects().
        if (child->isOutOfFlowPositioned())
            continue;

        // Outline of an element continuation or anonymous block continuation is added when we iterate the continuation chain.
        // See LayoutBlock::addOutlineRects() and LayoutInline::addOutlineRects().
        if (child->isElementContinuation()
            || (child->isLayoutBlockFlow() && toLayoutBlockFlow(child)->isAnonymousBlockContinuation()))
            continue;

        addOutlineRectsForDescendant(*child, rects, additionalOffset, includeBlockOverflows);
    }
}

void LayoutBoxModelObject::addOutlineRectsForDescendant(const LayoutObject& descendant, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, IncludeBlockVisualOverflowOrNot includeBlockOverflows) const
{
    if (descendant.isText() || descendant.isListMarker())
        return;

    if (descendant.hasLayer()) {
        Vector<LayoutRect> layerOutlineRects;
        descendant.addOutlineRects(layerOutlineRects, LayoutPoint(), includeBlockOverflows);
        descendant.localToAncestorRects(layerOutlineRects, this, LayoutPoint(), additionalOffset);
        rects.appendVector(layerOutlineRects);
        return;
    }

    if (descendant.isBox()) {
        descendant.addOutlineRects(rects, additionalOffset + toLayoutBox(descendant).locationOffset(), includeBlockOverflows);
        return;
    }

    if (descendant.isLayoutInline()) {
        // As an optimization, an ancestor has added rects for its line boxes covering descendants'
        // line boxes, so descendants don't need to add line boxes again. For example, if the parent
        // is a LayoutBlock, it adds rects for its RootOutlineBoxes which cover the line boxes of
        // this LayoutInline. So the LayoutInline needs to add rects for children and continuations only.
        toLayoutInline(descendant).addOutlineRectsForChildrenAndContinuations(rects, additionalOffset, includeBlockOverflows);
        return;
    }

    descendant.addOutlineRects(rects, additionalOffset, includeBlockOverflows);
}

bool LayoutBoxModelObject::calculateHasBoxDecorations() const
{
    const ComputedStyle& styleToUse = styleRef();
    return hasBackground() || styleToUse.hasBorderDecoration() || styleToUse.hasAppearance() || styleToUse.boxShadow();
}

bool LayoutBoxModelObject::hasNonEmptyLayoutSize() const
{
    for (const LayoutBoxModelObject* root = this; root; root = root->continuation()) {
        for (const LayoutObject* object = root; object; object = object->nextInPreOrder(object)) {
            if (object->isBox()) {
                const LayoutBox& box = toLayoutBox(*object);
                if (box.logicalHeight() && box.logicalWidth())
                    return true;
            } else if (object->isLayoutInline()) {
                const LayoutInline& layoutInline = toLayoutInline(*object);
                if (!layoutInline.linesBoundingBox().isEmpty())
                    return true;
            } else {
                ASSERT(object->isText());
            }
        }
    }
    return false;
}

void LayoutBoxModelObject::updateFromStyle()
{
    const ComputedStyle& styleToUse = styleRef();
    setHasBoxDecorationBackground(calculateHasBoxDecorations());
    setInline(styleToUse.isDisplayInlineType());
    setPositionState(styleToUse.position());
    setHorizontalWritingMode(styleToUse.isHorizontalWritingMode());
}

LayoutBlock* LayoutBoxModelObject::containingBlockForAutoHeightDetection(Length logicalHeight) const
{
    // For percentage heights: The percentage is calculated with respect to the height of the generated box's
    // containing block. If the height of the containing block is not specified explicitly (i.e., it depends
    // on content height), and this element is not absolutely positioned,  the used height is calculated
    // as if 'auto' was specified.
    if (!logicalHeight.hasPercent() || isOutOfFlowPositioned())
        return nullptr;

    // Anonymous block boxes are ignored when resolving percentage values that would refer to it:
    // the closest non-anonymous ancestor box is used instead.
    LayoutBlock* cb = containingBlock();
    while (cb->isAnonymous())
        cb = cb->containingBlock();

    // Matching LayoutBox::percentageLogicalHeightIsResolvableFromBlock() by
    // ignoring table cell's attribute value, where it says that table cells violate
    // what the CSS spec says to do with heights. Basically we
    // don't care if the cell specified a height or not.
    if (cb->isTableCell())
        return nullptr;

    // Match LayoutBox::availableLogicalHeightUsing by special casing
    // the layout view. The available height is taken from the frame.
    if (cb->isLayoutView())
        return nullptr;

    if (cb->isOutOfFlowPositioned() && !cb->style()->logicalTop().isAuto() && !cb->style()->logicalBottom().isAuto())
        return nullptr;

    return cb;
}

bool LayoutBoxModelObject::hasAutoHeightOrContainingBlockWithAutoHeight() const
{
    const LayoutBox* thisBox = isBox() ? toLayoutBox(this) : nullptr;
    Length logicalHeightLength = style()->logicalHeight();
    LayoutBlock* cb = containingBlockForAutoHeightDetection(logicalHeightLength);
    if (logicalHeightLength.hasPercent() && cb && isBox())
        cb->addPercentHeightDescendant(const_cast<LayoutBox*>(toLayoutBox(this)));
    if (thisBox && thisBox->isFlexItem()) {
        LayoutFlexibleBox& flexBox = toLayoutFlexibleBox(*parent());
        if (flexBox.childLogicalHeightForPercentageResolution(*thisBox) != LayoutUnit(-1))
            return false;
    }
    if (logicalHeightLength.isAuto())
        return true;

    if (document().inQuirksMode())
        return false;

    // If the height of the containing block computes to 'auto', then it hasn't been 'specified explicitly'.
    if (cb)
        return cb->hasAutoHeightOrContainingBlockWithAutoHeight();
    return false;
}

LayoutSize LayoutBoxModelObject::relativePositionOffset() const
{
    LayoutSize offset = accumulateInFlowPositionOffsets();

    LayoutBlock* containingBlock = this->containingBlock();

    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockLogicalWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && !containingBlock->style()->isLeftToRightDirection())
            offset.setWidth(-valueForLength(style()->right(), containingBlock->availableWidth()));
        else
            offset.expand(valueForLength(style()->left(), containingBlock->availableWidth()), LayoutUnit());
    } else if (!style()->right().isAuto()) {
        offset.expand(-valueForLength(style()->right(), containingBlock->availableWidth()), LayoutUnit());
    }

    // If the containing block of a relatively positioned element does not
    // specify a height, a percentage top or bottom offset should be resolved as
    // auto. An exception to this is if the containing block has the WinIE quirk
    // where <html> and <body> assume the size of the viewport. In this case,
    // calculate the percent offset based on this height.
    // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
    if (!style()->top().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style()->top().hasPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(LayoutUnit(), valueForLength(style()->top(), containingBlock->availableHeight()));

    else if (!style()->bottom().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style()->bottom().hasPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(LayoutUnit(), -valueForLength(style()->bottom(), containingBlock->availableHeight()));

    return offset;
}

void LayoutBoxModelObject::updateStickyPositionConstraints() const
{
    // TODO(flackr): This method is reasonably complicated and should have some direct unit testing.
    const FloatSize constrainingSize = computeStickyConstrainingRect().size();

    PaintLayerScrollableArea* scrollableArea = layer()->ancestorOverflowLayer()->getScrollableArea();
    StickyPositionScrollingConstraints constraints;
    FloatSize skippedContainersOffset;
    LayoutBlock* containingBlock = this->containingBlock();
    // Skip anonymous containing blocks.
    while (containingBlock->isAnonymous()) {
        skippedContainersOffset += toFloatSize(FloatPoint(containingBlock->frameRect().location()));
        containingBlock = containingBlock->containingBlock();
    }
    LayoutBox* scrollAncestor = layer()->ancestorOverflowLayer()->isRootLayer() ? nullptr : toLayoutBox(layer()->ancestorOverflowLayer()->layoutObject());

    LayoutRect containerContentRect = containingBlock->contentBoxRect();
    LayoutUnit maxWidth = containingBlock->availableLogicalWidth();

    // Sticky positioned element ignore any override logical width on the containing block (as they don't call
    // containingBlockLogicalWidthForContent). It's unclear whether this is totally fine.
    // Compute the container-relative area within which the sticky element is allowed to move.
    containerContentRect.contractEdges(
        minimumValueForLength(style()->marginTop(), maxWidth),
        minimumValueForLength(style()->marginRight(), maxWidth),
        minimumValueForLength(style()->marginBottom(), maxWidth),
        minimumValueForLength(style()->marginLeft(), maxWidth));

    // Map to the scroll ancestor.
    constraints.setScrollContainerRelativeContainingBlockRect(containingBlock->localToAncestorQuad(FloatRect(containerContentRect), scrollAncestor).boundingBox());

    FloatRect stickyBoxRect = isLayoutInline()
        ? FloatRect(toLayoutInline(this)->linesBoundingBox())
        : FloatRect(toLayoutBox(this)->frameRect());
    FloatRect flippedStickyBoxRect = stickyBoxRect;
    containingBlock->flipForWritingMode(flippedStickyBoxRect);
    FloatPoint stickyLocation = flippedStickyBoxRect.location() + skippedContainersOffset;

    // TODO(flackr): Unfortunate to call localToAncestorQuad again, but we can't just offset from the previously computed rect if there are transforms.
    // Map to the scroll ancestor.
    FloatRect scrollContainerRelativeContainerFrame = containingBlock->localToAncestorQuad(FloatRect(FloatPoint(), FloatSize(containingBlock->size())), scrollAncestor).boundingBox();

    // If the containing block is our scroll ancestor, its location will not include the scroll offset which we need to include as
    // part of the sticky box rect so we include it here.
    if (containingBlock->hasOverflowClip()) {
        FloatSize scrollOffset(toFloatSize(containingBlock->layer()->getScrollableArea()->adjustedScrollOffset()));
        stickyLocation -= scrollOffset;
    }

    constraints.setScrollContainerRelativeStickyBoxRect(FloatRect(scrollContainerRelativeContainerFrame.location() + toFloatSize(stickyLocation), flippedStickyBoxRect.size()));

    // We skip the right or top sticky offset if there is not enough space to honor both the left/right or top/bottom offsets.
    LayoutUnit horizontalOffsets = minimumValueForLength(style()->right(), LayoutUnit(constrainingSize.width())) +
        minimumValueForLength(style()->left(), LayoutUnit(constrainingSize.width()));
    bool skipRight = false;
    bool skipLeft = false;
    if (!style()->left().isAuto() && !style()->right().isAuto()) {
        if (horizontalOffsets > containerContentRect.width()
            || horizontalOffsets + containerContentRect.width() > constrainingSize.width()) {
            skipRight = style()->isLeftToRightDirection();
            skipLeft = !skipRight;
        }
    }

    if (!style()->left().isAuto() && !skipLeft) {
        constraints.setLeftOffset(minimumValueForLength(style()->left(), LayoutUnit(constrainingSize.width())));
        constraints.addAnchorEdge(StickyPositionScrollingConstraints::AnchorEdgeLeft);
    }

    if (!style()->right().isAuto() && !skipRight) {
        constraints.setRightOffset(minimumValueForLength(style()->right(), LayoutUnit(constrainingSize.width())));
        constraints.addAnchorEdge(StickyPositionScrollingConstraints::AnchorEdgeRight);
    }

    bool skipBottom = false;
    // TODO(flackr): Exclude top or bottom edge offset depending on the writing mode when related
    // sections are fixed in spec: http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
    LayoutUnit verticalOffsets = minimumValueForLength(style()->top(), LayoutUnit(constrainingSize.height())) +
        minimumValueForLength(style()->bottom(), LayoutUnit(constrainingSize.height()));
    if (!style()->top().isAuto() && !style()->bottom().isAuto()) {
        if (verticalOffsets > containerContentRect.height()
            || verticalOffsets + containerContentRect.height() > constrainingSize.height()) {
            skipBottom = true;
        }
    }

    if (!style()->top().isAuto()) {
        constraints.setTopOffset(minimumValueForLength(style()->top(), LayoutUnit(constrainingSize.height())));
        constraints.addAnchorEdge(StickyPositionScrollingConstraints::AnchorEdgeTop);
    }

    if (!style()->bottom().isAuto() && !skipBottom) {
        constraints.setBottomOffset(minimumValueForLength(style()->bottom(), LayoutUnit(constrainingSize.height())));
        constraints.addAnchorEdge(StickyPositionScrollingConstraints::AnchorEdgeBottom);
    }
    scrollableArea->stickyConstraintsMap().set(layer(), constraints);
}

FloatRect LayoutBoxModelObject::computeStickyConstrainingRect() const
{
    if (layer()->ancestorOverflowLayer()->isRootLayer())
        return view()->frameView()->visibleContentRect();

    LayoutBox* enclosingClippingBox = toLayoutBox(layer()->ancestorOverflowLayer()->layoutObject());
    FloatRect constrainingRect;
    constrainingRect = FloatRect(enclosingClippingBox->overflowClipRect(LayoutPoint()));
    constrainingRect.move(enclosingClippingBox->paddingLeft(), enclosingClippingBox->paddingTop());
    constrainingRect.contract(FloatSize(enclosingClippingBox->paddingLeft() + enclosingClippingBox->paddingRight(),
        enclosingClippingBox->paddingTop() + enclosingClippingBox->paddingBottom()));
    return constrainingRect;
}

LayoutSize LayoutBoxModelObject::stickyPositionOffset() const
{
    const PaintLayer* ancestorOverflowLayer = layer()->ancestorOverflowLayer();
    // TODO: Force compositing input update if we ask for offset before compositing inputs have been computed?
    if (!ancestorOverflowLayer)
        return LayoutSize();
    FloatRect constrainingRect = computeStickyConstrainingRect();
    PaintLayerScrollableArea* scrollableArea = ancestorOverflowLayer->getScrollableArea();

    // The sticky offset is physical, so we can just return the delta computed in absolute coords (though it may be wrong with transforms).
    // TODO: Force compositing input update if we ask for offset with stale compositing inputs.
    if (!scrollableArea->stickyConstraintsMap().contains(layer()))
        return LayoutSize();
    return LayoutSize(scrollableArea->stickyConstraintsMap().get(layer()).computeStickyOffset(constrainingRect));
}

LayoutPoint LayoutBoxModelObject::adjustedPositionRelativeTo(const LayoutPoint& startPoint, const Element* element) const
{
    // If the element is the HTML body element or doesn't have a parent
    // return 0 and stop this algorithm.
    if (isBody() || !parent())
        return LayoutPoint();

    LayoutPoint referencePoint = startPoint;
    referencePoint.move(parent()->columnOffset(referencePoint));

    // If the base element is null, return the distance between the canvas origin and
    // the left border edge of the element and stop this algorithm.
    if (!element)
        return referencePoint;

    if (const LayoutBoxModelObject* offsetParent = element->layoutBoxModelObject()) {
        if (offsetParent->isBox() && !offsetParent->isBody())
            referencePoint.move(-toLayoutBox(offsetParent)->borderLeft(), -toLayoutBox(offsetParent)->borderTop());
        if (!isOutOfFlowPositioned() || flowThreadContainingBlock()) {
            if (isInFlowPositioned())
                referencePoint.move(offsetForInFlowPosition());

            LayoutObject* current;
            for (current = parent(); current != offsetParent && current->parent(); current = current->parent()) {
                // FIXME: What are we supposed to do inside SVG content?
                if (!isOutOfFlowPositioned()) {
                    if (current->isBox() && !current->isTableRow())
                        referencePoint.moveBy(toLayoutBox(current)->topLeftLocation());
                    referencePoint.move(current->parent()->columnOffset(referencePoint));
                }
            }

            if (offsetParent->isBox() && offsetParent->isBody() && !offsetParent->isPositioned())
                referencePoint.moveBy(toLayoutBox(offsetParent)->topLeftLocation());
        }
    }

    return referencePoint;
}

LayoutSize LayoutBoxModelObject::offsetForInFlowPosition() const
{
    if (isRelPositioned())
        return relativePositionOffset();

    if (isStickyPositioned())
        return stickyPositionOffset();

    return LayoutSize();
}

LayoutUnit LayoutBoxModelObject::offsetLeft(const Element* parent) const
{
    // Note that LayoutInline and LayoutBox override this to pass a different
    // startPoint to adjustedPositionRelativeTo.
    return adjustedPositionRelativeTo(LayoutPoint(), parent).x();
}

LayoutUnit LayoutBoxModelObject::offsetTop(const Element* parent) const
{
    // Note that LayoutInline and LayoutBox override this to pass a different
    // startPoint to adjustedPositionRelativeTo.
    return adjustedPositionRelativeTo(LayoutPoint(), parent).y();
}

int LayoutBoxModelObject::pixelSnappedOffsetWidth(const Element* parent) const
{
    return snapSizeToPixel(offsetWidth(), offsetLeft(parent));
}

int LayoutBoxModelObject::pixelSnappedOffsetHeight(const Element* parent) const
{
    return snapSizeToPixel(offsetHeight(), offsetTop(parent));
}

LayoutUnit LayoutBoxModelObject::computedCSSPadding(const Length& padding) const
{
    LayoutUnit w;
    if (padding.hasPercent())
        w = containingBlockLogicalWidthForContent();
    return minimumValueForLength(padding, w);
}

bool LayoutBoxModelObject::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, const InlineFlowBox* inlineFlowBox) const
{
    if (bleedAvoidance != BackgroundBleedNone)
        return false;

    if (style()->hasAppearance())
        return false;

    const ShadowList* shadowList = style()->boxShadow();
    if (!shadowList)
        return false;

    bool hasOneNormalBoxShadow = false;
    size_t shadowCount = shadowList->shadows().size();
    for (size_t i = 0; i < shadowCount; ++i) {
        const ShadowData& currentShadow = shadowList->shadows()[i];
        if (currentShadow.style() != Normal)
            continue;

        if (hasOneNormalBoxShadow)
            return false;
        hasOneNormalBoxShadow = true;

        if (currentShadow.spread())
            return false;
    }

    if (!hasOneNormalBoxShadow)
        return false;

    Color backgroundColor = resolveColor(CSSPropertyBackgroundColor);
    if (backgroundColor.hasAlpha())
        return false;

    const FillLayer* lastBackgroundLayer = &style()->backgroundLayers();
    for (const FillLayer* next = lastBackgroundLayer->next(); next; next = lastBackgroundLayer->next())
        lastBackgroundLayer = next;

    if (lastBackgroundLayer->clip() != BorderFillBox)
        return false;

    if (lastBackgroundLayer->image() && style()->hasBorderRadius())
        return false;

    if (inlineFlowBox && !inlineFlowBox->boxShadowCanBeAppliedToBackground(*lastBackgroundLayer))
        return false;

    if (hasOverflowClip() && lastBackgroundLayer->attachment() == LocalBackgroundAttachment)
        return false;

    return true;
}



LayoutUnit LayoutBoxModelObject::containingBlockLogicalWidthForContent() const
{
    return containingBlock()->availableLogicalWidth();
}

LayoutBoxModelObject* LayoutBoxModelObject::continuation() const
{
    return (!continuationMap) ? nullptr : continuationMap->get(this);
}

void LayoutBoxModelObject::setContinuation(LayoutBoxModelObject* continuation)
{
    if (continuation) {
        ASSERT(continuation->isLayoutInline() || continuation->isLayoutBlockFlow());
        if (!continuationMap)
            continuationMap = new ContinuationMap;
        continuationMap->set(this, continuation);
    } else {
        if (continuationMap)
            continuationMap->remove(this);
    }
}

void LayoutBoxModelObject::computeLayerHitTestRects(LayerHitTestRects& rects) const
{
    LayoutObject::computeLayerHitTestRects(rects);

    // If there is a continuation then we need to consult it here, since this is
    // the root of the tree walk and it wouldn't otherwise get picked up.
    // Continuations should always be siblings in the tree, so any others should
    // get picked up already by the tree walk.
    if (continuation())
        continuation()->computeLayerHitTestRects(rects);
}

LayoutRect LayoutBoxModelObject::localCaretRectForEmptyElement(LayoutUnit width, LayoutUnit textIndentOffset)
{
    ASSERT(!slowFirstChild());

    // FIXME: This does not take into account either :first-line or :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed and this kludge is not called any more. So only the caret size
    // of an empty :first-line'd block is wrong. I think we can live with that.
    const ComputedStyle& currentStyle = firstLineStyleRef();

    enum CaretAlignment { AlignLeft, AlignRight, AlignCenter };

    CaretAlignment alignment = AlignLeft;

    switch (currentStyle.textAlign()) {
    case LEFT:
    case WEBKIT_LEFT:
        break;
    case CENTER:
    case WEBKIT_CENTER:
        alignment = AlignCenter;
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        alignment = AlignRight;
        break;
    case JUSTIFY:
    case TASTART:
        if (!currentStyle.isLeftToRightDirection())
            alignment = AlignRight;
        break;
    case TAEND:
        if (currentStyle.isLeftToRightDirection())
            alignment = AlignRight;
        break;
    }

    LayoutUnit x = borderLeft() + paddingLeft();
    LayoutUnit maxX = width - borderRight() - paddingRight();

    switch (alignment) {
    case AlignLeft:
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset;
        break;
    case AlignCenter:
        x = (x + maxX) / 2;
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset / 2;
        else
            x -= textIndentOffset / 2;
        break;
    case AlignRight:
        x = maxX - caretWidth();
        if (!currentStyle.isLeftToRightDirection())
            x -= textIndentOffset;
        break;
    }
    x = std::min(x, (maxX - caretWidth()).clampNegativeToZero());

    const Font& font = style()->font();
    const SimpleFontData* fontData = font.primaryFont();
    LayoutUnit height;
    // crbug.com/595692 This check should not be needed but sometimes
    // primaryFont is null.
    if (fontData)
        height = LayoutUnit(fontData->getFontMetrics().height());
    LayoutUnit verticalSpace = lineHeight(true, currentStyle.isHorizontalWritingMode() ? HorizontalLine : VerticalLine,  PositionOfInteriorLineBoxes) - height;
    LayoutUnit y = paddingTop() + borderTop() + (verticalSpace / 2);
    return currentStyle.isHorizontalWritingMode() ? LayoutRect(x, y, caretWidth(), height) : LayoutRect(y, x, height, caretWidth());
}

const LayoutObject* LayoutBoxModelObject::pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    LayoutObject* container = this->container(ancestorToStopAt, &ancestorSkipped);
    if (!container)
        return nullptr;

    bool isInline = isLayoutInline();
    bool isFixedPos = !isInline && style()->position() == FixedPosition;
    bool containsFixedPosition = canContainFixedPositionObjects();

    LayoutSize adjustmentForSkippedAncestor;
    if (ancestorSkipped) {
        // There can't be a transform between paintInvalidationContainer and ancestorToStopAt, because transforms create containers, so it should be safe
        // to just subtract the delta between the ancestor and ancestorToStopAt.
        adjustmentForSkippedAncestor = -ancestorToStopAt->offsetFromAncestorContainer(container);
    }

    LayoutSize containerOffset = offsetFromContainer(container);
    bool offsetDependsOnPoint;
    if (isLayoutFlowThread()) {
        containerOffset += columnOffset(LayoutPoint());
        offsetDependsOnPoint = true;
    } else {
        offsetDependsOnPoint = container->style()->isFlippedBlocksWritingMode() && container->isBox();
    }

    bool preserve3D = container->style()->preserves3D() || style()->preserves3D();
    GeometryInfoFlags flags = 0;
    if (preserve3D)
        flags |= AccumulatingTransform;
    if (offsetDependsOnPoint)
        flags |= IsNonUniform;
    if (isFixedPos)
        flags |= IsFixedPosition;
    if (containsFixedPosition)
        flags |= ContainsFixedPosition;
    if (shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        t.translateRight(adjustmentForSkippedAncestor.width().toFloat(), adjustmentForSkippedAncestor.height().toFloat());
        geometryMap.push(this, t, flags, LayoutSize());
    } else {
        containerOffset += adjustmentForSkippedAncestor;
        geometryMap.push(this, containerOffset, flags, LayoutSize());
    }

    return ancestorSkipped ? ancestorToStopAt : container;
}

void LayoutBoxModelObject::moveChildTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* child, LayoutObject* beforeChild, bool fullRemoveInsert)
{
    // We assume that callers have cleared their positioned objects list for child moves (!fullRemoveInsert) so the
    // positioned layoutObject maps don't become stale. It would be too slow to do the map lookup on each call.
    ASSERT(!fullRemoveInsert || !isLayoutBlock() || !toLayoutBlock(this)->hasPositionedObjects());

    ASSERT(this == child->parent());
    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());

    // If a child is moving from a block-flow to an inline-flow parent then any floats currently intruding into
    // the child can no longer do so. This can happen if a block becomes floating or out-of-flow and is moved
    // to an anonymous block. Remove all floats from their float-lists immediately as markAllDescendantsWithFloatsForLayout
    // won't attempt to remove floats from parents that have inline-flow if we try later.
    if (child->isLayoutBlockFlow() && toBoxModelObject->childrenInline() && !childrenInline()) {
        toLayoutBlockFlow(child)->removeFloatingObjectsFromDescendants();
        ASSERT(!toLayoutBlockFlow(child)->containsFloats());
    }

    if (fullRemoveInsert && isLayoutBlock() && child->isBox())
        toLayoutBox(child)->removeFromPercentHeightContainer();

    if (fullRemoveInsert && (toBoxModelObject->isLayoutBlock() || toBoxModelObject->isLayoutInline())) {
        // Takes care of adding the new child correctly if toBlock and fromBlock
        // have different kind of children (block vs inline).
        toBoxModelObject->addChild(virtualChildren()->removeChildNode(this, child), beforeChild);
    } else {
        toBoxModelObject->virtualChildren()->insertChildNode(toBoxModelObject, virtualChildren()->removeChildNode(this, child, fullRemoveInsert), beforeChild, fullRemoveInsert);
    }
}

void LayoutBoxModelObject::moveChildrenTo(LayoutBoxModelObject* toBoxModelObject, LayoutObject* startChild, LayoutObject* endChild, LayoutObject* beforeChild, bool fullRemoveInsert)
{
    // This condition is rarely hit since this function is usually called on
    // anonymous blocks which can no longer carry positioned objects (see r120761)
    // or when fullRemoveInsert is false.
    if (fullRemoveInsert && isLayoutBlock()) {
        LayoutBlock* block = toLayoutBlock(this);
        block->removePositionedObjects(nullptr);
        block->removeFromPercentHeightContainer();
        if (block->isLayoutBlockFlow())
            toLayoutBlockFlow(block)->removeFloatingObjects();
    }

    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
    for (LayoutObject* child = startChild; child && child != endChild; ) {
        // Save our next sibling as moveChildTo will clear it.
        LayoutObject* nextSibling = child->nextSibling();
        moveChildTo(toBoxModelObject, child, beforeChild, fullRemoveInsert);
        child = nextSibling;
    }
}

bool LayoutBoxModelObject::backgroundStolenForBeingBody(const ComputedStyle* rootElementStyle) const
{
    // http://www.w3.org/TR/css3-background/#body-background
    // If the root element is <html> with no background, and a <body> child element exists,
    // the root element steals the first <body> child element's background.
    if (!isBody())
        return false;

    Element* rootElement = document().documentElement();
    if (!isHTMLHtmlElement(rootElement))
        return false;

    if (!rootElementStyle)
        rootElementStyle = rootElement->ensureComputedStyle();
    if (rootElementStyle->hasBackground())
        return false;

    if (node() != document().firstBodyElement())
        return false;

    return true;
}

} // namespace blink
