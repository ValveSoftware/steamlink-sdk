// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObservation.h"

#include "core/dom/ElementRareData.h"
#include "core/dom/IntersectionObserver.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer, Element& target, bool shouldReportRootBounds)
    : m_observer(observer)
    , m_target(&target)
    , m_shouldReportRootBounds(shouldReportRootBounds)
    , m_lastThresholdIndex(0)
{
}

void IntersectionObservation::applyRootMargin(LayoutRect& rect) const
{
    if (m_shouldReportRootBounds)
        m_observer->applyRootMargin(rect);
}

void IntersectionObservation::initializeGeometry(IntersectionGeometry& geometry) const
{
    initializeTargetRect(geometry.targetRect);
    geometry.intersectionRect = geometry.targetRect;
    initializeRootRect(geometry.rootRect);
    geometry.doesIntersect = true;
}

void IntersectionObservation::initializeTargetRect(LayoutRect& rect) const
{
    DCHECK(m_target);
    LayoutObject* targetLayoutObject = target()->layoutObject();
    DCHECK(targetLayoutObject && targetLayoutObject->isBoxModelObject());
    rect = LayoutRect(toLayoutBoxModelObject(targetLayoutObject)->borderBoundingBox());
}

void IntersectionObservation::initializeRootRect(LayoutRect& rect) const
{
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    if (rootLayoutObject->isLayoutView())
        rect = LayoutRect(toLayoutView(rootLayoutObject)->frameView()->visibleContentRect());
    else if (rootLayoutObject->isBox() && rootLayoutObject->hasOverflowClip())
        rect = LayoutRect(toLayoutBox(rootLayoutObject)->contentBoxRect());
    else
        rect = LayoutRect(toLayoutBoxModelObject(rootLayoutObject)->borderBoundingBox());
    applyRootMargin(rect);
}

void IntersectionObservation::clipToRoot(IntersectionGeometry& geometry) const
{
    // Map and clip rect into root element coordinates.
    // TODO(szager): the writing mode flipping needs a test.
    DCHECK(m_target);
    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    LayoutObject* targetLayoutObject = target()->layoutObject();

    geometry.doesIntersect = targetLayoutObject->mapToVisualRectInAncestorSpace(toLayoutBoxModelObject(rootLayoutObject), geometry.intersectionRect, EdgeInclusive);
    if (!geometry.doesIntersect)
        return;
    LayoutRect rootClipRect(geometry.rootRect);
    toLayoutBox(rootLayoutObject)->flipForWritingMode(rootClipRect);
    geometry.doesIntersect &= geometry.intersectionRect.inclusiveIntersect(rootClipRect);
}

static void mapRectUpToDocument(LayoutRect& rect, const LayoutObject& layoutObject, const Document& document)
{
    FloatQuad mappedQuad = layoutObject.localToAbsoluteQuad(
        FloatQuad(FloatRect(rect)),
        UseTransforms | ApplyContainerFlip);
    rect = LayoutRect(mappedQuad.boundingBox());
}

static void mapRectDownToDocument(LayoutRect& rect, LayoutBoxModelObject& layoutObject, const Document& document)
{
    FloatQuad mappedQuad = document.layoutView()->ancestorToLocalQuad(
        &layoutObject,
        FloatQuad(FloatRect(rect)),
        UseTransforms | ApplyContainerFlip | TraverseDocumentBoundaries);
    rect = LayoutRect(mappedQuad.boundingBox());
}

void IntersectionObservation::mapTargetRectToTargetFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& targetLayoutObject = *target()->layoutObject();
    Document& targetDocument = target()->document();
    LayoutSize scrollPosition = LayoutSize(targetDocument.view()->scrollOffset());
    mapRectUpToDocument(rect, targetLayoutObject, targetDocument);
    rect.move(-scrollPosition);
}

void IntersectionObservation::mapRootRectToRootFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& rootLayoutObject = *m_observer->rootLayoutObject();
    Document& rootDocument = rootLayoutObject.document();
    LayoutSize scrollPosition = LayoutSize(rootDocument.view()->scrollOffset());
    mapRectUpToDocument(rect, rootLayoutObject, rootLayoutObject.document());
    rect.move(-scrollPosition);
}

void IntersectionObservation::mapRootRectToTargetFrameCoordinates(LayoutRect& rect) const
{
    LayoutObject& rootLayoutObject = *m_observer->rootLayoutObject();
    Document& targetDocument = target()->document();
    LayoutSize scrollPosition = LayoutSize(targetDocument.view()->scrollOffset());

    if (&targetDocument == &rootLayoutObject.document())
        mapRectUpToDocument(rect, rootLayoutObject, targetDocument);
    else
        mapRectDownToDocument(rect, toLayoutBoxModelObject(rootLayoutObject), targetDocument);

    rect.move(-scrollPosition);
}

static bool isContainingBlockChainDescendant(LayoutObject* descendant, LayoutObject* ancestor)
{
    LocalFrame* ancestorFrame = ancestor->document().frame();
    LocalFrame* descendantFrame = descendant->document().frame();

    if (ancestor->isLayoutView())
        return descendantFrame && descendantFrame->tree().top() == ancestorFrame;

    if (ancestorFrame != descendantFrame)
        return false;

    while (descendant && descendant != ancestor)
        descendant = descendant->containingBlock();
    return descendant;
}

bool IntersectionObservation::computeGeometry(IntersectionGeometry& geometry) const
{
    // In the first few lines here, before initializeGeometry is called, "return true"
    // effectively means "if the previous observed state was that root and target were
    // intersecting, then generate a notification indicating that they are no longer
    // intersecting."  This happens, for example, when root or target is removed from the
    // DOM tree and not reinserted before the next frame is generated, or display:none
    // is set on the root or target.
    Element* targetElement = target();
    if (!targetElement)
        return false;
    if (!targetElement->inShadowIncludingDocument())
        return true;
    DCHECK(m_observer);
    Element* rootElement = m_observer->root();
    if (rootElement && !rootElement->inShadowIncludingDocument())
        return true;

    LayoutObject* rootLayoutObject = m_observer->rootLayoutObject();
    if (!rootLayoutObject || !rootLayoutObject->isBoxModelObject())
        return true;
    // TODO(szager): Support SVG
    LayoutObject* targetLayoutObject = targetElement->layoutObject();
    if (!targetLayoutObject)
        return true;
    if (!targetLayoutObject->isBoxModelObject() && !targetLayoutObject->isText())
        return true;
    if (!isContainingBlockChainDescendant(targetLayoutObject, rootLayoutObject))
        return true;

    initializeGeometry(geometry);

    clipToRoot(geometry);

    mapTargetRectToTargetFrameCoordinates(geometry.targetRect);

    if (geometry.doesIntersect)
        mapRootRectToTargetFrameCoordinates(geometry.intersectionRect);
    else
        geometry.intersectionRect = LayoutRect();

    // Small optimization: if we're not going to report root bounds, don't bother
    // transforming them to the frame.
    if (m_shouldReportRootBounds)
        mapRootRectToRootFrameCoordinates(geometry.rootRect);

    return true;
}

void IntersectionObservation::computeIntersectionObservations(DOMHighResTimeStamp timestamp)
{
    IntersectionGeometry geometry;
    if (!computeGeometry(geometry))
        return;

    // Some corner cases for threshold index:
    //   - If target rect is zero area, because it has zero width and/or zero height,
    //     only two states are recognized:
    //     - 0 means not intersecting.
    //     - 1 means intersecting.
    //     No other threshold crossings are possible.
    //   - Otherwise:
    //     - If root and target do not intersect, the threshold index is 0.
    //     - If root and target intersect but the intersection has zero-area (i.e., they
    //       have a coincident edge or corner), we consider the intersection to have
    //       "crossed" a zero threshold, but not crossed any non-zero threshold.
    unsigned newThresholdIndex;
    float newVisibleRatio = 0;
    if (geometry.targetRect.isEmpty()) {
        newThresholdIndex = geometry.doesIntersect ? 1 : 0;
    } else if (!geometry.doesIntersect) {
        newThresholdIndex = 0;
    } else {
        float intersectionArea = geometry.intersectionRect.size().width().toFloat() * geometry.intersectionRect.size().height().toFloat();
        float targetArea = geometry.targetRect.size().width().toFloat() * geometry.targetRect.size().height().toFloat();
        newVisibleRatio = intersectionArea / targetArea;
        newThresholdIndex = observer().firstThresholdGreaterThan(newVisibleRatio);
    }
    if (m_lastThresholdIndex != newThresholdIndex) {
        IntRect snappedRootBounds = pixelSnappedIntRect(geometry.rootRect);
        IntRect* rootBoundsPointer = m_shouldReportRootBounds ? &snappedRootBounds : nullptr;
        IntersectionObserverEntry* newEntry = new IntersectionObserverEntry(
            timestamp,
            newVisibleRatio,
            pixelSnappedIntRect(geometry.targetRect),
            rootBoundsPointer,
            pixelSnappedIntRect(geometry.intersectionRect),
            target());
        observer().enqueueIntersectionObserverEntry(*newEntry);
        setLastThresholdIndex(newThresholdIndex);
    }
}

void IntersectionObservation::disconnect()
{
    IntersectionObserver* observer = m_observer;
    clearRootAndRemoveFromTarget();
    observer->removeObservation(*this);
}

void IntersectionObservation::clearRootAndRemoveFromTarget()
{
    if (m_target)
        target()->ensureIntersectionObserverData().removeObservation(observer());
    m_observer.clear();
}

DEFINE_TRACE(IntersectionObservation)
{
    visitor->trace(m_observer);
    visitor->trace(m_target);
}

} // namespace blink
