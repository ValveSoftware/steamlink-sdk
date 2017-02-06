/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 * Copyright (C) 2012 Google Inc.
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
 */

#include "core/layout/svg/LayoutSVGText.h"

#include "core/editing/PositionWithAffinity.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutState.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/layout/svg/SVGTextLayoutAttributesBuilder.h"
#include "core/layout/svg/line/SVGRootInlineBox.h"
#include "core/paint/SVGTextPainter.h"
#include "core/style/ShadowList.h"
#include "core/svg/SVGTextElement.h"
#include "platform/FloatConversion.h"
#include "platform/geometry/FloatQuad.h"

namespace blink {

namespace {

const LayoutSVGText* findTextRoot(const LayoutObject* start)
{
    ASSERT(start);
    for (; start; start = start->parent()) {
        if (start->isSVGText())
            return toLayoutSVGText(start);
    }
    return nullptr;
}

} // namespace

LayoutSVGText::LayoutSVGText(SVGTextElement* node)
    : LayoutSVGBlock(node)
    , m_needsReordering(false)
    , m_needsPositioningValuesUpdate(false)
    , m_needsTransformUpdate(true)
    , m_needsTextMetricsUpdate(false)
{
}

LayoutSVGText::~LayoutSVGText()
{
    ASSERT(m_descendantTextNodes.isEmpty());
}

void LayoutSVGText::willBeDestroyed()
{
    m_descendantTextNodes.clear();

    LayoutSVGBlock::willBeDestroyed();
}

bool LayoutSVGText::isChildAllowed(LayoutObject* child, const ComputedStyle&) const
{
    return child->isSVGInline() || (child->isText() && SVGLayoutSupport::isLayoutableTextNode(child));
}

LayoutSVGText* LayoutSVGText::locateLayoutSVGTextAncestor(LayoutObject* start)
{
    return const_cast<LayoutSVGText*>(findTextRoot(start));
}

const LayoutSVGText* LayoutSVGText::locateLayoutSVGTextAncestor(const LayoutObject* start)
{
    return findTextRoot(start);
}

static inline void collectDescendantTextNodes(LayoutSVGText& textRoot, Vector<LayoutSVGInlineText*>& descendantTextNodes)
{
    for (LayoutObject* descendant = textRoot.firstChild(); descendant; descendant = descendant->nextInPreOrder(&textRoot)) {
        if (descendant->isSVGInlineText())
            descendantTextNodes.append(toLayoutSVGInlineText(descendant));
    }
}

void LayoutSVGText::invalidatePositioningValues(LayoutInvalidationReasonForTracing reason)
{
    m_descendantTextNodes.clear();
    setNeedsPositioningValuesUpdate();
    setNeedsLayoutAndFullPaintInvalidation(reason);
}

void LayoutSVGText::subtreeChildWasAdded()
{
    if (beingDestroyed() || !everHadLayout()) {
        ASSERT(m_descendantTextNodes.isEmpty());
        return;
    }
    if (documentBeingDestroyed())
        return;

    // The positioning elements cache depends on the size of each text layoutObject in the
    // subtree. If this changes, clear the cache. It will be rebuilt on the next layout.
    invalidatePositioningValues(LayoutInvalidationReason::ChildChanged);
    setNeedsTextMetricsUpdate();
}

void LayoutSVGText::subtreeChildWillBeRemoved()
{
    if (beingDestroyed() || !everHadLayout()) {
        ASSERT(m_descendantTextNodes.isEmpty());
        return;
    }

    // The positioning elements cache depends on the size of each text layoutObject in the
    // subtree. If this changes, clear the cache. It will be rebuilt on the next layout.
    invalidatePositioningValues(LayoutInvalidationReason::ChildChanged);
    setNeedsTextMetricsUpdate();
}

void LayoutSVGText::subtreeTextDidChange()
{
    ASSERT(!beingDestroyed());
    if (!everHadLayout()) {
        ASSERT(m_descendantTextNodes.isEmpty());
        return;
    }

    // The positioning elements cache depends on the size of each text object in
    // the subtree. If this changes, clear the cache and mark it for rebuilding
    // in the next layout.
    invalidatePositioningValues(LayoutInvalidationReason::TextChanged);
    setNeedsTextMetricsUpdate();
}

static inline void updateFontAndMetrics(LayoutSVGText& textRoot)
{
    bool lastCharacterWasWhiteSpace = true;
    for (LayoutObject* descendant = textRoot.firstChild(); descendant; descendant = descendant->nextInPreOrder(&textRoot)) {
        if (!descendant->isSVGInlineText())
            continue;
        LayoutSVGInlineText& text = toLayoutSVGInlineText(*descendant);
        text.updateScaledFont();
        text.updateMetricsList(lastCharacterWasWhiteSpace);
    }
}

static inline void checkDescendantTextNodeConsistency(LayoutSVGText& text, Vector<LayoutSVGInlineText*>& expectedDescendantTextNodes)
{
#if ENABLE(ASSERT)
    Vector<LayoutSVGInlineText*> newDescendantTextNodes;
    collectDescendantTextNodes(text, newDescendantTextNodes);
    ASSERT(newDescendantTextNodes == expectedDescendantTextNodes);
#endif
}

void LayoutSVGText::layout()
{
    ASSERT(needsLayout());
    // This flag is set and reset as needed only within this function.
    ASSERT(!m_needsReordering);
    LayoutAnalyzer::Scope analyzer(*this);

    bool updateParentBoundaries = false;
    if (m_needsTransformUpdate) {
        m_localTransform = toSVGTextElement(node())->calculateAnimatedLocalTransform();
        m_needsTransformUpdate = false;
        updateParentBoundaries = true;
    }

    // When laying out initially, build the character data map and propagate
    // resulting layout attributes to all LayoutSVGInlineText children in the
    // subtree.
    if (!everHadLayout()) {
        m_needsPositioningValuesUpdate = true;
        m_needsTextMetricsUpdate = true;
    }

    // If the root layout size changed (eg. window size changes), or the screen
    // scale factor has changed, then recompute the on-screen font size. Since
    // the computation of layout attributes uses the text metrics, we need to
    // update them before updating the layout attributes.
    if (m_needsTextMetricsUpdate) {
        updateFontAndMetrics(*this);
        m_needsTextMetricsUpdate = false;
        updateParentBoundaries = true;
    }

    // When the x/y/dx/dy/rotate lists change, we need to recompute the layout
    // attributes.
    if (m_needsPositioningValuesUpdate) {
        m_descendantTextNodes.clear();
        collectDescendantTextNodes(*this, m_descendantTextNodes);

        SVGTextLayoutAttributesBuilder(*this).buildLayoutAttributes();

        m_needsPositioningValuesUpdate = false;
        m_needsReordering = true;
        updateParentBoundaries = true;
    }

    checkDescendantTextNodeConsistency(*this, m_descendantTextNodes);

    // Reduced version of LayoutBlock::layoutBlock(), which only takes care of SVG text.
    // All if branches that could cause early exit in LayoutBlocks layoutBlock() method are turned into assertions.
    ASSERT(!isInline());
    ASSERT(!simplifiedLayout());
    ASSERT(!scrollsOverflow());
    ASSERT(!hasControlClip());
    ASSERT(!positionedObjects());
    ASSERT(!isAnonymousBlock());

    if (!firstChild())
        setChildrenInline(true);

    // FIXME: We need to find a way to only layout the child boxes, if needed.
    FloatRect oldBoundaries = objectBoundingBox();
    ASSERT(childrenInline());

    rebuildFloatsFromIntruding();

    LayoutUnit beforeEdge = borderBefore() + paddingBefore();
    LayoutUnit afterEdge = borderAfter() + paddingAfter() + scrollbarLogicalHeight();
    setLogicalHeight(beforeEdge);

    LayoutState state(*this, locationOffset());
    layoutInlineChildren(true, afterEdge);

    m_needsReordering = false;

    // If we don't have any line boxes, then make sure the frame rect is still cleared.
    if (!firstLineBox())
        setFrameRect(LayoutRect());

    m_overflow.reset();
    addVisualEffectOverflow();

    if (!updateParentBoundaries)
        updateParentBoundaries = oldBoundaries != objectBoundingBox();

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // If our bounds changed, notify the parents.
    if (updateParentBoundaries)
        LayoutSVGBlock::setNeedsBoundariesUpdate();

    ASSERT(!m_needsReordering);
    ASSERT(!m_needsTransformUpdate);
    ASSERT(!m_needsTextMetricsUpdate);
    ASSERT(!m_needsPositioningValuesUpdate);
    clearNeedsLayout();
}

RootInlineBox* LayoutSVGText::createRootInlineBox()
{
    RootInlineBox* box = new SVGRootInlineBox(LineLayoutItem(this));
    box->setHasVirtualLogicalHeight();
    return box;
}

bool LayoutSVGText::nodeAtFloatPoint(HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the foreground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING, result.hitTestRequest(), style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        if ((hitRules.canHitBoundingBox && !objectBoundingBox().isEmpty())
            || (hitRules.canHitStroke && (style()->svgStyle().hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (style()->svgStyle().hasFill() || !hitRules.requireFill))) {
            FloatPoint localPoint;
            if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(this, localToSVGParentTransform(), pointInParent, localPoint))
                return false;

            HitTestLocation hitTestLocation(localPoint);
            if (LayoutBlock::nodeAtPoint(result, hitTestLocation, LayoutPoint(), hitTestAction))
                return true;

            // Consider the bounding box if requested.
            if (hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint)) {
                const LayoutPoint& localLayoutPoint = roundedLayoutPoint(localPoint);
                updateHitTestResult(result, localLayoutPoint);
                if (result.addNodeToListBasedTestResult(node(), localLayoutPoint) == StopHitTesting)
                    return true;
            }
        }
    }

    return false;
}

PositionWithAffinity LayoutSVGText::positionForPoint(const LayoutPoint& pointInContents)
{
    RootInlineBox* rootBox = firstRootBox();
    if (!rootBox)
        return createPositionWithAffinity(0);

    LayoutPoint clippedPointInContents(pointInContents);
    clippedPointInContents.clampNegativeToZero();

    ASSERT(!rootBox->nextRootBox());
    ASSERT(childrenInline());

    InlineBox* closestBox = toSVGRootInlineBox(rootBox)->closestLeafChildForPosition(clippedPointInContents);
    if (!closestBox)
        return createPositionWithAffinity(0);

    return closestBox->getLineLayoutItem().positionForPoint(LayoutPoint(clippedPointInContents.x(), closestBox->y()));
}

void LayoutSVGText::absoluteQuads(Vector<FloatQuad>& quads) const
{
    quads.append(localToAbsoluteQuad(strokeBoundingBox()));
}

void LayoutSVGText::paint(const PaintInfo& paintInfo, const LayoutPoint&) const
{
    SVGTextPainter(*this).paint(paintInfo);
}

FloatRect LayoutSVGText::strokeBoundingBox() const
{
    FloatRect strokeBoundaries = objectBoundingBox();
    const SVGComputedStyle& svgStyle = style()->svgStyle();
    if (!svgStyle.hasStroke())
        return strokeBoundaries;

    ASSERT(node());
    ASSERT(node()->isSVGElement());
    SVGLengthContext lengthContext(toSVGElement(node()));
    strokeBoundaries.inflate(lengthContext.valueForLength(svgStyle.strokeWidth()));
    return strokeBoundaries;
}

FloatRect LayoutSVGText::paintInvalidationRectInLocalSVGCoordinates() const
{
    FloatRect paintInvalidationRect = strokeBoundingBox();
    SVGLayoutSupport::intersectPaintInvalidationRectWithResources(this, paintInvalidationRect);

    if (const ShadowList* textShadow = style()->textShadow())
        textShadow->adjustRectForShadow(paintInvalidationRect);

    return paintInvalidationRect;
}

bool LayoutSVGText::isObjectBoundingBoxValid() const
{
    // If we don't have any line boxes, then consider the bbox invalid.
    return firstLineBox();
}

void LayoutSVGText::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    LayoutSVGBlock::addChild(child, beforeChild);

    SVGResourcesCache::clientWasAddedToTree(child, child->styleRef());
    subtreeChildWasAdded();
}

void LayoutSVGText::removeChild(LayoutObject* child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    subtreeChildWillBeRemoved();

    LayoutSVGBlock::removeChild(child);
}

void LayoutSVGText::invalidateTreeIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    // TODO(wangxianzhu): Verify if the inherited LayoutBoxModelObject::invalidateTreeIfNeeded()
    // is applicable here. If yes, remove this overriding method.
    LayoutObject::invalidateTreeIfNeeded(paintInvalidationState);
}

} // namespace blink
