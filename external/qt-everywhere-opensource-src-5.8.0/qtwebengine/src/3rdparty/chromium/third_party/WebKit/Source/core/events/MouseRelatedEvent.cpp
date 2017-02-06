/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/events/MouseRelatedEvent.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/PaintLayer.h"

namespace blink {

MouseRelatedEvent::MouseRelatedEvent()
    : m_positionType(PositionType::Position)
    , m_hasCachedRelativePosition(false)
{
}

static LayoutSize contentsScrollOffset(AbstractView* abstractView)
{
    if (!abstractView || !abstractView->isLocalDOMWindow())
        return LayoutSize();
    LocalFrame* frame = toLocalDOMWindow(abstractView)->frame();
    if (!frame)
        return LayoutSize();
    FrameView* frameView = frame->view();
    if (!frameView)
        return LayoutSize();
    float scaleFactor = frame->pageZoomFactor();
    return LayoutSize(frameView->scrollX() / scaleFactor, frameView->scrollY() / scaleFactor);
}

MouseRelatedEvent::MouseRelatedEvent(const AtomicString& eventType, bool canBubble, bool cancelable,
    AbstractView* abstractView, int detail, const IntPoint& screenLocation,
    const IntPoint& rootFrameLocation, const IntPoint& movementDelta, PlatformEvent::Modifiers modifiers,
    double platformTimeStamp, PositionType positionType, InputDeviceCapabilities* sourceCapabilities)
    : UIEventWithKeyState(eventType, canBubble, cancelable, abstractView, detail, modifiers, platformTimeStamp, sourceCapabilities)
    , m_screenLocation(screenLocation)
    , m_movementDelta(movementDelta)
    , m_positionType(positionType)
{
    LayoutPoint adjustedPageLocation;
    LayoutPoint scrollPosition;

    LocalFrame* frame = view() && view()->isLocalDOMWindow() ? toLocalDOMWindow(view())->frame() : nullptr;
    if (frame && hasPosition()) {
        if (FrameView* frameView = frame->view()) {
            scrollPosition = frameView->scrollPosition();
            adjustedPageLocation = frameView->rootFrameToContents(rootFrameLocation);
            float scaleFactor = 1 / frame->pageZoomFactor();
            if (scaleFactor != 1.0f) {
                adjustedPageLocation.scale(scaleFactor, scaleFactor);
                scrollPosition.scale(scaleFactor, scaleFactor);
            }
        }
    }

    m_clientLocation = adjustedPageLocation - toLayoutSize(scrollPosition);
    m_pageLocation = adjustedPageLocation;

    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.
    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

MouseRelatedEvent::MouseRelatedEvent(const AtomicString& eventType, const MouseEventInit& initializer)
    : UIEventWithKeyState(eventType, initializer)
    , m_screenLocation(IntPoint(initializer.screenX(), initializer.screenY()))
    , m_movementDelta(IntPoint(initializer.movementX(), initializer.movementY()))
    , m_positionType(PositionType::Position)
{
    initCoordinates(IntPoint(initializer.clientX(), initializer.clientY()));
}

void MouseRelatedEvent::initCoordinates(const LayoutPoint& clientLocation)
{
    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.
    m_clientLocation = clientLocation;
    m_pageLocation = clientLocation + contentsScrollOffset(view());

    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

static float pageZoomFactor(const UIEvent* event)
{
    if (!event->view() || !event->view()->isLocalDOMWindow())
        return 1;
    LocalFrame* frame = toLocalDOMWindow(event->view())->frame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void MouseRelatedEvent::computePageLocation()
{
    float scaleFactor = pageZoomFactor(this);
    setAbsoluteLocation(roundedLayoutPoint(FloatPoint(pageX() * scaleFactor, pageY() * scaleFactor)));
}

void MouseRelatedEvent::receivedTarget()
{
    m_hasCachedRelativePosition = false;
}

static const LayoutObject* findTargetLayoutObject(Node*& targetNode)
{
    LayoutObject* layoutObject = targetNode->layoutObject();
    if (!layoutObject || !layoutObject->isSVG())
        return layoutObject;
    // If this is an SVG node, compute the offset to the padding box of the
    // outermost SVG root (== the closest ancestor that has a CSS layout box.)
    while (!layoutObject->isSVGRoot())
        layoutObject = layoutObject->parent();
    // Update the target node to point to the SVG root.
    targetNode = layoutObject->node();
    DCHECK(!targetNode
        || (targetNode->isSVGElement() && toSVGElement(*targetNode).isOutermostSVGSVGElement()));
    return layoutObject;
}

void MouseRelatedEvent::computeRelativePosition()
{
    Node* targetNode = target() ? target()->toNode() : nullptr;
    if (!targetNode)
        return;

    // Compute coordinates that are based on the target.
    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    // Must have an updated layout tree for this math to work correctly.
    targetNode->document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Adjust offsetLocation to be relative to the target's padding box.
    if (const LayoutObject* layoutObject = findTargetLayoutObject(targetNode)) {
        FloatPoint localPos = layoutObject->absoluteToLocal(FloatPoint(absoluteLocation()), UseTransforms);

        // Adding this here to address crbug.com/570666. Basically we'd like to
        // find the local coordinates relative to the padding box not the border box.
        if (layoutObject->isBoxModelObject()) {
            const LayoutBoxModelObject* layoutBox = toLayoutBoxModelObject(layoutObject);
            localPos.move(-layoutBox->borderLeft(), -layoutBox->borderTop());
        }

        m_offsetLocation = roundedLayoutPoint(localPos);
        float scaleFactor = 1 / pageZoomFactor(this);
        if (scaleFactor != 1.0f)
            m_offsetLocation.scale(scaleFactor, scaleFactor);
    }

    // Adjust layerLocation to be relative to the layer.
    // FIXME: event.layerX and event.layerY are poorly defined,
    // and probably don't always correspond to PaintLayer offsets.
    // https://bugs.webkit.org/show_bug.cgi?id=21868
    Node* n = targetNode;
    while (n && !n->layoutObject())
        n = n->parentNode();

    if (n) {
        // FIXME: This logic is a wrong implementation of convertToLayerCoords.
        for (PaintLayer* layer = n->layoutObject()->enclosingLayer(); layer; layer = layer->parent())
            m_layerLocation -= toLayoutSize(layer->location());
    }

    m_hasCachedRelativePosition = true;
}

int MouseRelatedEvent::layerX()
{
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return m_layerLocation.x();
}

int MouseRelatedEvent::layerY()
{
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return m_layerLocation.y();
}

int MouseRelatedEvent::offsetX()
{
    if (!hasPosition())
        return 0;
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return roundToInt(m_offsetLocation.x());
}

int MouseRelatedEvent::offsetY()
{
    if (!hasPosition())
        return 0;
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return roundToInt(m_offsetLocation.y());
}

int MouseRelatedEvent::pageX() const
{
    return m_pageLocation.x();
}

int MouseRelatedEvent::pageY() const
{
    return m_pageLocation.y();
}

int MouseRelatedEvent::x() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientLocation.x();
}

int MouseRelatedEvent::y() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientLocation.y();
}

DEFINE_TRACE(MouseRelatedEvent)
{
    UIEventWithKeyState::trace(visitor);
}

} // namespace blink
