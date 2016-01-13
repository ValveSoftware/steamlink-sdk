/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/SpatialNavigation.h"

#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/rendering/RenderLayer.h"
#include "platform/geometry/IntRect.h"

namespace WebCore {

using namespace HTMLNames;

static RectsAlignment alignmentForRects(FocusType, const LayoutRect&, const LayoutRect&, const LayoutSize& viewSize);
static bool areRectsFullyAligned(FocusType, const LayoutRect&, const LayoutRect&);
static bool areRectsPartiallyAligned(FocusType, const LayoutRect&, const LayoutRect&);
static bool areRectsMoreThanFullScreenApart(FocusType, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize);
static bool isRectInDirection(FocusType, const LayoutRect&, const LayoutRect&);
static void deflateIfOverlapped(LayoutRect&, LayoutRect&);
static LayoutRect rectToAbsoluteCoordinates(LocalFrame* initialFrame, const LayoutRect&);
static bool isScrollableNode(const Node*);

FocusCandidate::FocusCandidate(Node* node, FocusType type)
    : visibleNode(0)
    , focusableNode(0)
    , enclosingScrollableBox(0)
    , distance(maxDistance())
    , parentDistance(maxDistance())
    , alignment(None)
    , parentAlignment(None)
    , isOffscreen(true)
    , isOffscreenAfterScrolling(true)
{
    ASSERT(node);
    ASSERT(node->isElementNode());

    if (isHTMLAreaElement(*node)) {
        HTMLAreaElement& area = toHTMLAreaElement(*node);
        HTMLImageElement* image = area.imageElement();
        if (!image || !image->renderer())
            return;

        visibleNode = image;
        rect = virtualRectForAreaElementAndDirection(area, type);
    } else {
        if (!node->renderer())
            return;

        visibleNode = node;
        rect = nodeRectInAbsoluteCoordinates(node, true /* ignore border */);
    }

    focusableNode = node;
    isOffscreen = hasOffscreenRect(visibleNode);
    isOffscreenAfterScrolling = hasOffscreenRect(visibleNode, type);
}

bool isSpatialNavigationEnabled(const LocalFrame* frame)
{
    return (frame && frame->settings() && frame->settings()->spatialNavigationEnabled());
}

static RectsAlignment alignmentForRects(FocusType type, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize)
{
    // If we found a node in full alignment, but it is too far away, ignore it.
    if (areRectsMoreThanFullScreenApart(type, curRect, targetRect, viewSize))
        return None;

    if (areRectsFullyAligned(type, curRect, targetRect))
        return Full;

    if (areRectsPartiallyAligned(type, curRect, targetRect))
        return Partial;

    return None;
}

static inline bool isHorizontalMove(FocusType type)
{
    return type == FocusTypeLeft || type == FocusTypeRight;
}

static inline LayoutUnit start(FocusType type, const LayoutRect& rect)
{
    return isHorizontalMove(type) ? rect.y() : rect.x();
}

static inline LayoutUnit middle(FocusType type, const LayoutRect& rect)
{
    LayoutPoint center(rect.center());
    return isHorizontalMove(type) ? center.y(): center.x();
}

static inline LayoutUnit end(FocusType type, const LayoutRect& rect)
{
    return isHorizontalMove(type) ? rect.maxY() : rect.maxX();
}

// This method checks if rects |a| and |b| are fully aligned either vertically or
// horizontally. In general, rects whose central point falls between the top or
// bottom of each other are considered fully aligned.
// Rects that match this criteria are preferable target nodes in move focus changing
// operations.
// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static bool areRectsFullyAligned(FocusType type, const LayoutRect& a, const LayoutRect& b)
{
    LayoutUnit aStart, bStart, aEnd, bEnd;

    switch (type) {
    case FocusTypeLeft:
        aStart = a.x();
        bEnd = b.x();
        break;
    case FocusTypeRight:
        aStart = b.x();
        bEnd = a.x();
        break;
    case FocusTypeUp:
        aStart = a.y();
        bEnd = b.y();
        break;
    case FocusTypeDown:
        aStart = b.y();
        bEnd = a.y();
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    if (aStart < bEnd)
        return false;

    aStart = start(type, a);
    bStart = start(type, b);

    LayoutUnit aMiddle = middle(type, a);
    LayoutUnit bMiddle = middle(type, b);

    aEnd = end(type, a);
    bEnd = end(type, b);

    // Picture of the totally aligned logic:
    //
    //     Horizontal    Vertical        Horizontal     Vertical
    //  ****************************  *****************************
    //  *  _          *   _ _ _ _  *  *         _   *      _ _    *
    //  * |_|     _   *  |_|_|_|_| *  *  _     |_|  *     |_|_|   *
    //  * |_|....|_|  *      .     *  * |_|....|_|  *       .     *
    //  * |_|    |_| (1)     .     *  * |_|    |_| (2)      .     *
    //  * |_|         *     _._    *  *        |_|  *    _ _._ _  *
    //  *             *    |_|_|   *  *             *   |_|_|_|_| *
    //  *             *            *  *             *             *
    //  ****************************  *****************************

    return (bMiddle >= aStart && bMiddle <= aEnd) // (1)
        || (aMiddle >= bStart && aMiddle <= bEnd); // (2)
}

// This method checks if rects |a| and |b| are partially aligned either vertically or
// horizontally. In general, rects whose either of edges falls between the top or
// bottom of each other are considered partially-aligned.
// This is a separate set of conditions from "fully-aligned" and do not include cases
// that satisfy the former.
// * a = Current focused node's rect.
// * b = Focus candidate node's rect.
static bool areRectsPartiallyAligned(FocusType type, const LayoutRect& a, const LayoutRect& b)
{
    LayoutUnit aStart  = start(type, a);
    LayoutUnit bStart  = start(type, b);
    LayoutUnit aEnd = end(type, a);
    LayoutUnit bEnd = end(type, b);

    // Picture of the partially aligned logic:
    //
    //    Horizontal       Vertical
    // ********************************
    // *  _            *   _ _ _      *
    // * |_|           *  |_|_|_|     *
    // * |_|.... _     *      . .     *
    // * |_|    |_|    *      . .     *
    // * |_|....|_|    *      ._._ _  *
    // *        |_|    *      |_|_|_| *
    // *        |_|    *              *
    // *               *              *
    // ********************************
    //
    // ... and variants of the above cases.
    return (bStart >= aStart && bStart <= aEnd)
        || (bEnd >= aStart && bEnd <= aEnd);
}

static bool areRectsMoreThanFullScreenApart(FocusType type, const LayoutRect& curRect, const LayoutRect& targetRect, const LayoutSize& viewSize)
{
    ASSERT(isRectInDirection(type, curRect, targetRect));

    switch (type) {
    case FocusTypeLeft:
        return curRect.x() - targetRect.maxX() > viewSize.width();
    case FocusTypeRight:
        return targetRect.x() - curRect.maxX() > viewSize.width();
    case FocusTypeUp:
        return curRect.y() - targetRect.maxY() > viewSize.height();
    case FocusTypeDown:
        return targetRect.y() - curRect.maxY() > viewSize.height();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

// Return true if rect |a| is below |b|. False otherwise.
// For overlapping rects, |a| is considered to be below |b|
// if both edges of |a| are below the respective ones of |b|
static inline bool below(const LayoutRect& a, const LayoutRect& b)
{
    return a.y() >= b.maxY()
        || (a.y() >= b.y() && a.maxY() > b.maxY());
}

// Return true if rect |a| is on the right of |b|. False otherwise.
// For overlapping rects, |a| is considered to be on the right of |b|
// if both edges of |a| are on the right of the respective ones of |b|
static inline bool rightOf(const LayoutRect& a, const LayoutRect& b)
{
    return a.x() >= b.maxX()
        || (a.x() >= b.x() && a.maxX() > b.maxX());
}

static bool isRectInDirection(FocusType type, const LayoutRect& curRect, const LayoutRect& targetRect)
{
    switch (type) {
    case FocusTypeLeft:
        return rightOf(curRect, targetRect);
    case FocusTypeRight:
        return rightOf(targetRect, curRect);
    case FocusTypeUp:
        return below(curRect, targetRect);
    case FocusTypeDown:
        return below(targetRect, curRect);
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

// Checks if |node| is offscreen the visible area (viewport) of its container
// document. In case it is, one can scroll in direction or take any different
// desired action later on.
bool hasOffscreenRect(Node* node, FocusType type)
{
    // Get the FrameView in which |node| is (which means the current viewport if |node|
    // is not in an inner document), so we can check if its content rect is visible
    // before we actually move the focus to it.
    FrameView* frameView = node->document().view();
    if (!frameView)
        return true;

    ASSERT(!frameView->needsLayout());

    LayoutRect containerViewportRect = frameView->visibleContentRect();
    // We want to select a node if it is currently off screen, but will be
    // exposed after we scroll. Adjust the viewport to post-scrolling position.
    // If the container has overflow:hidden, we cannot scroll, so we do not pass direction
    // and we do not adjust for scrolling.
    switch (type) {
    case FocusTypeLeft:
        containerViewportRect.setX(containerViewportRect.x() - ScrollableArea::pixelsPerLineStep());
        containerViewportRect.setWidth(containerViewportRect.width() + ScrollableArea::pixelsPerLineStep());
        break;
    case FocusTypeRight:
        containerViewportRect.setWidth(containerViewportRect.width() + ScrollableArea::pixelsPerLineStep());
        break;
    case FocusTypeUp:
        containerViewportRect.setY(containerViewportRect.y() - ScrollableArea::pixelsPerLineStep());
        containerViewportRect.setHeight(containerViewportRect.height() + ScrollableArea::pixelsPerLineStep());
        break;
    case FocusTypeDown:
        containerViewportRect.setHeight(containerViewportRect.height() + ScrollableArea::pixelsPerLineStep());
        break;
    default:
        break;
    }

    RenderObject* render = node->renderer();
    if (!render)
        return true;

    LayoutRect rect(render->absoluteClippedOverflowRect());
    if (rect.isEmpty())
        return true;

    return !containerViewportRect.intersects(rect);
}

bool scrollInDirection(LocalFrame* frame, FocusType type)
{
    ASSERT(frame);

    if (frame && canScrollInDirection(frame->document(), type)) {
        LayoutUnit dx = 0;
        LayoutUnit dy = 0;
        switch (type) {
        case FocusTypeLeft:
            dx = - ScrollableArea::pixelsPerLineStep();
            break;
        case FocusTypeRight:
            dx = ScrollableArea::pixelsPerLineStep();
            break;
        case FocusTypeUp:
            dy = - ScrollableArea::pixelsPerLineStep();
            break;
        case FocusTypeDown:
            dy = ScrollableArea::pixelsPerLineStep();
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }

        frame->view()->scrollBy(IntSize(dx, dy));
        return true;
    }
    return false;
}

bool scrollInDirection(Node* container, FocusType type)
{
    ASSERT(container);
    if (container->isDocumentNode())
        return scrollInDirection(toDocument(container)->frame(), type);

    if (!container->renderBox())
        return false;

    if (canScrollInDirection(container, type)) {
        LayoutUnit dx = 0;
        LayoutUnit dy = 0;
        switch (type) {
        case FocusTypeLeft:
            dx = - std::min<LayoutUnit>(ScrollableArea::pixelsPerLineStep(), container->renderBox()->scrollLeft());
            break;
        case FocusTypeRight:
            ASSERT(container->renderBox()->scrollWidth() > (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            dx = std::min<LayoutUnit>(ScrollableArea::pixelsPerLineStep(), container->renderBox()->scrollWidth() - (container->renderBox()->scrollLeft() + container->renderBox()->clientWidth()));
            break;
        case FocusTypeUp:
            dy = - std::min<LayoutUnit>(ScrollableArea::pixelsPerLineStep(), container->renderBox()->scrollTop());
            break;
        case FocusTypeDown:
            ASSERT(container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            dy = std::min<LayoutUnit>(ScrollableArea::pixelsPerLineStep(), container->renderBox()->scrollHeight() - (container->renderBox()->scrollTop() + container->renderBox()->clientHeight()));
            break;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }

        container->renderBox()->scrollByRecursively(IntSize(dx, dy));
        return true;
    }

    return false;
}

static void deflateIfOverlapped(LayoutRect& a, LayoutRect& b)
{
    if (!a.intersects(b) || a.contains(b) || b.contains(a))
        return;

    LayoutUnit deflateFactor = -fudgeFactor();

    // Avoid negative width or height values.
    if ((a.width() + 2 * deflateFactor > 0) && (a.height() + 2 * deflateFactor > 0))
        a.inflate(deflateFactor);

    if ((b.width() + 2 * deflateFactor > 0) && (b.height() + 2 * deflateFactor > 0))
        b.inflate(deflateFactor);
}

bool isScrollableNode(const Node* node)
{
    ASSERT(!node->isDocumentNode());

    if (!node)
        return false;

    if (RenderObject* renderer = node->renderer())
        return renderer->isBox() && toRenderBox(renderer)->canBeScrolledAndHasScrollableArea() && node->hasChildren();

    return false;
}

Node* scrollableEnclosingBoxOrParentFrameForNodeInDirection(FocusType type, Node* node)
{
    ASSERT(node);
    Node* parent = node;
    do {
        // FIXME: Spatial navigation is broken for OOPI.
        if (parent->isDocumentNode())
            parent = toDocument(parent)->frame()->deprecatedLocalOwner();
        else
            parent = parent->parentOrShadowHostNode();
    } while (parent && !canScrollInDirection(parent, type) && !parent->isDocumentNode());

    return parent;
}

bool canScrollInDirection(const Node* container, FocusType type)
{
    ASSERT(container);
    if (container->isDocumentNode())
        return canScrollInDirection(toDocument(container)->frame(), type);

    if (!isScrollableNode(container))
        return false;

    switch (type) {
    case FocusTypeLeft:
        return (container->renderer()->style()->overflowX() != OHIDDEN && container->renderBox()->scrollLeft() > 0);
    case FocusTypeUp:
        return (container->renderer()->style()->overflowY() != OHIDDEN && container->renderBox()->scrollTop() > 0);
    case FocusTypeRight:
        return (container->renderer()->style()->overflowX() != OHIDDEN && container->renderBox()->scrollLeft() + container->renderBox()->clientWidth() < container->renderBox()->scrollWidth());
    case FocusTypeDown:
        return (container->renderer()->style()->overflowY() != OHIDDEN && container->renderBox()->scrollTop() + container->renderBox()->clientHeight() < container->renderBox()->scrollHeight());
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool canScrollInDirection(const LocalFrame* frame, FocusType type)
{
    if (!frame->view())
        return false;
    ScrollbarMode verticalMode;
    ScrollbarMode horizontalMode;
    frame->view()->calculateScrollbarModesForLayoutAndSetViewportRenderer(horizontalMode, verticalMode);
    if ((type == FocusTypeLeft || type == FocusTypeRight) && ScrollbarAlwaysOff == horizontalMode)
        return false;
    if ((type == FocusTypeUp || type == FocusTypeDown) &&  ScrollbarAlwaysOff == verticalMode)
        return false;
    LayoutSize size = frame->view()->contentsSize();
    LayoutSize offset = frame->view()->scrollOffset();
    LayoutRect rect = frame->view()->visibleContentRect(IncludeScrollbars);

    switch (type) {
    case FocusTypeLeft:
        return offset.width() > 0;
    case FocusTypeUp:
        return offset.height() > 0;
    case FocusTypeRight:
        return rect.width() + offset.width() < size.width();
    case FocusTypeDown:
        return rect.height() + offset.height() < size.height();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static LayoutRect rectToAbsoluteCoordinates(LocalFrame* initialFrame, const LayoutRect& initialRect)
{
    LayoutRect rect = initialRect;
    for (Frame* frame = initialFrame; frame; frame = frame->tree().parent()) {
        if (!frame->isLocalFrame())
            continue;
        // FIXME: Spatial navigation is broken for OOPI.
        if (Element* element = frame->deprecatedLocalOwner()) {
            do {
                rect.move(element->offsetLeft(), element->offsetTop());
            } while ((element = element->offsetParent()));
            rect.move((-toLocalFrame(frame)->view()->scrollOffset()));
        }
    }
    return rect;
}

LayoutRect nodeRectInAbsoluteCoordinates(Node* node, bool ignoreBorder)
{
    ASSERT(node && node->renderer() && !node->document().view()->needsLayout());

    if (node->isDocumentNode())
        return frameRectInAbsoluteCoordinates(toDocument(node)->frame());
    LayoutRect rect = rectToAbsoluteCoordinates(node->document().frame(), node->boundingBox());

    // For authors that use border instead of outline in their CSS, we compensate by ignoring the border when calculating
    // the rect of the focused element.
    if (ignoreBorder) {
        rect.move(node->renderer()->style()->borderLeftWidth(), node->renderer()->style()->borderTopWidth());
        rect.setWidth(rect.width() - node->renderer()->style()->borderLeftWidth() - node->renderer()->style()->borderRightWidth());
        rect.setHeight(rect.height() - node->renderer()->style()->borderTopWidth() - node->renderer()->style()->borderBottomWidth());
    }
    return rect;
}

LayoutRect frameRectInAbsoluteCoordinates(LocalFrame* frame)
{
    return rectToAbsoluteCoordinates(frame, frame->view()->visibleContentRect());
}

// This method calculates the exitPoint from the startingRect and the entryPoint into the candidate rect.
// The line between those 2 points is the closest distance between the 2 rects.
// Takes care of overlapping rects, defining points so that the distance between them
// is zero where necessary
void entryAndExitPointsForDirection(FocusType type, const LayoutRect& startingRect, const LayoutRect& potentialRect, LayoutPoint& exitPoint, LayoutPoint& entryPoint)
{
    switch (type) {
    case FocusTypeLeft:
        exitPoint.setX(startingRect.x());
        if (potentialRect.maxX() < startingRect.x())
            entryPoint.setX(potentialRect.maxX());
        else
            entryPoint.setX(startingRect.x());
        break;
    case FocusTypeUp:
        exitPoint.setY(startingRect.y());
        if (potentialRect.maxY() < startingRect.y())
            entryPoint.setY(potentialRect.maxY());
        else
            entryPoint.setY(startingRect.y());
        break;
    case FocusTypeRight:
        exitPoint.setX(startingRect.maxX());
        if (potentialRect.x() > startingRect.maxX())
            entryPoint.setX(potentialRect.x());
        else
            entryPoint.setX(startingRect.maxX());
        break;
    case FocusTypeDown:
        exitPoint.setY(startingRect.maxY());
        if (potentialRect.y() > startingRect.maxY())
            entryPoint.setY(potentialRect.y());
        else
            entryPoint.setY(startingRect.maxY());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    switch (type) {
    case FocusTypeLeft:
    case FocusTypeRight:
        if (below(startingRect, potentialRect)) {
            exitPoint.setY(startingRect.y());
            if (potentialRect.maxY() < startingRect.y())
                entryPoint.setY(potentialRect.maxY());
            else
                entryPoint.setY(startingRect.y());
        } else if (below(potentialRect, startingRect)) {
            exitPoint.setY(startingRect.maxY());
            if (potentialRect.y() > startingRect.maxY())
                entryPoint.setY(potentialRect.y());
            else
                entryPoint.setY(startingRect.maxY());
        } else {
            exitPoint.setY(max(startingRect.y(), potentialRect.y()));
            entryPoint.setY(exitPoint.y());
        }
        break;
    case FocusTypeUp:
    case FocusTypeDown:
        if (rightOf(startingRect, potentialRect)) {
            exitPoint.setX(startingRect.x());
            if (potentialRect.maxX() < startingRect.x())
                entryPoint.setX(potentialRect.maxX());
            else
                entryPoint.setX(startingRect.x());
        } else if (rightOf(potentialRect, startingRect)) {
            exitPoint.setX(startingRect.maxX());
            if (potentialRect.x() > startingRect.maxX())
                entryPoint.setX(potentialRect.x());
            else
                entryPoint.setX(startingRect.maxX());
        } else {
            exitPoint.setX(max(startingRect.x(), potentialRect.x()));
            entryPoint.setX(exitPoint.x());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool areElementsOnSameLine(const FocusCandidate& firstCandidate, const FocusCandidate& secondCandidate)
{
    if (firstCandidate.isNull() || secondCandidate.isNull())
        return false;

    if (!firstCandidate.visibleNode->renderer() || !secondCandidate.visibleNode->renderer())
        return false;

    if (!firstCandidate.rect.intersects(secondCandidate.rect))
        return false;

    if (isHTMLAreaElement(*firstCandidate.focusableNode) || isHTMLAreaElement(*secondCandidate.focusableNode))
        return false;

    if (!firstCandidate.visibleNode->renderer()->isRenderInline() || !secondCandidate.visibleNode->renderer()->isRenderInline())
        return false;

    if (firstCandidate.visibleNode->renderer()->containingBlock() != secondCandidate.visibleNode->renderer()->containingBlock())
        return false;

    return true;
}

void distanceDataForNode(FocusType type, const FocusCandidate& current, FocusCandidate& candidate)
{
    if (areElementsOnSameLine(current, candidate)) {
        if ((type == FocusTypeUp && current.rect.y() > candidate.rect.y()) || (type == FocusTypeDown && candidate.rect.y() > current.rect.y())) {
            candidate.distance = 0;
            candidate.alignment = Full;
            return;
        }
    }

    LayoutRect nodeRect = candidate.rect;
    LayoutRect currentRect = current.rect;
    deflateIfOverlapped(currentRect, nodeRect);

    if (!isRectInDirection(type, currentRect, nodeRect))
        return;

    LayoutPoint exitPoint;
    LayoutPoint entryPoint;
    entryAndExitPointsForDirection(type, currentRect, nodeRect, exitPoint, entryPoint);

    LayoutUnit xAxis = exitPoint.x() - entryPoint.x();
    LayoutUnit yAxis = exitPoint.y() - entryPoint.y();

    LayoutUnit navigationAxisDistance;
    LayoutUnit orthogonalAxisDistance;

    switch (type) {
    case FocusTypeLeft:
    case FocusTypeRight:
        navigationAxisDistance = xAxis.abs();
        orthogonalAxisDistance = yAxis.abs();
        break;
    case FocusTypeUp:
    case FocusTypeDown:
        navigationAxisDistance = yAxis.abs();
        orthogonalAxisDistance = xAxis.abs();
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    double euclidianDistancePow2 = (xAxis * xAxis + yAxis * yAxis).toDouble();
    LayoutRect intersectionRect = intersection(currentRect, nodeRect);
    double overlap = (intersectionRect.width() * intersectionRect.height()).toDouble();

    // Distance calculation is based on http://www.w3.org/TR/WICD/#focus-handling
    candidate.distance = sqrt(euclidianDistancePow2) + navigationAxisDistance+ orthogonalAxisDistance * 2 - sqrt(overlap);

    LayoutSize viewSize = candidate.visibleNode->document().page()->deprecatedLocalMainFrame()->view()->visibleContentRect().size();
    candidate.alignment = alignmentForRects(type, currentRect, nodeRect, viewSize);
}

bool canBeScrolledIntoView(FocusType type, const FocusCandidate& candidate)
{
    ASSERT(candidate.visibleNode && candidate.isOffscreen);
    LayoutRect candidateRect = candidate.rect;
    for (Node* parentNode = candidate.visibleNode->parentNode(); parentNode; parentNode = parentNode->parentNode()) {
        LayoutRect parentRect = nodeRectInAbsoluteCoordinates(parentNode);
        if (!candidateRect.intersects(parentRect)) {
            if (((type == FocusTypeLeft || type == FocusTypeRight) && parentNode->renderer()->style()->overflowX() == OHIDDEN)
                || ((type == FocusTypeUp || type == FocusTypeDown) && parentNode->renderer()->style()->overflowY() == OHIDDEN))
                return false;
        }
        if (parentNode == candidate.enclosingScrollableBox)
            return canScrollInDirection(parentNode, type);
    }
    return true;
}

// The starting rect is the rect of the focused node, in document coordinates.
// Compose a virtual starting rect if there is no focused node or if it is off screen.
// The virtual rect is the edge of the container or frame. We select which
// edge depending on the direction of the navigation.
LayoutRect virtualRectForDirection(FocusType type, const LayoutRect& startingRect, LayoutUnit width)
{
    LayoutRect virtualStartingRect = startingRect;
    switch (type) {
    case FocusTypeLeft:
        virtualStartingRect.setX(virtualStartingRect.maxX() - width);
        virtualStartingRect.setWidth(width);
        break;
    case FocusTypeUp:
        virtualStartingRect.setY(virtualStartingRect.maxY() - width);
        virtualStartingRect.setHeight(width);
        break;
    case FocusTypeRight:
        virtualStartingRect.setWidth(width);
        break;
    case FocusTypeDown:
        virtualStartingRect.setHeight(width);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return virtualStartingRect;
}

LayoutRect virtualRectForAreaElementAndDirection(HTMLAreaElement& area, FocusType type)
{
    ASSERT(area.imageElement());
    // Area elements tend to overlap more than other focusable elements. We flatten the rect of the area elements
    // to minimize the effect of overlapping areas.
    LayoutRect rect = virtualRectForDirection(type, rectToAbsoluteCoordinates(area.document().frame(), area.computeRect(area.imageElement()->renderer())), 1);
    return rect;
}

HTMLFrameOwnerElement* frameOwnerElement(FocusCandidate& candidate)
{
    return candidate.isFrameOwnerElement() ? toHTMLFrameOwnerElement(candidate.visibleNode) : 0;
};

} // namespace WebCore
