/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#include "platform/scroll/Scrollbar.h"

#include <algorithm>
#include "platform/HostWindow.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

Scrollbar::Scrollbar(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, HostWindow* hostWindow, ScrollbarTheme* theme)
    : m_scrollableArea(scrollableArea)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_theme(theme ? *theme : ScrollbarTheme::theme())
    , m_hostWindow(hostWindow)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_dragOrigin(0)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_scrollPos(0)
    , m_draggingDocument(false)
    , m_documentDragPos(0)
    , m_enabled(true)
    , m_scrollTimer(this, &Scrollbar::autoscrollTimerFired)
    , m_overlapsResizer(false)
    , m_elasticOverscroll(0)
    , m_trackNeedsRepaint(true)
    , m_thumbNeedsRepaint(true)
{
    m_theme.registerScrollbar(*this);

    // FIXME: This is ugly and would not be necessary if we fix cross-platform code to actually query for
    // scrollbar thickness and use it when sizing scrollbars (rather than leaving one dimension of the scrollbar
    // alone when sizing).
    int thickness = m_theme.scrollbarThickness(controlSize);
    if (m_hostWindow)
        thickness = m_hostWindow->windowToViewportScalar(thickness);
    Widget::setFrameRect(IntRect(0, 0, thickness, thickness));

    m_currentPos = scrollableAreaCurrentPos();
}

Scrollbar::~Scrollbar()
{
    m_theme.unregisterScrollbar(*this);
}

DEFINE_TRACE(Scrollbar)
{
    visitor->trace(m_scrollableArea);
    visitor->trace(m_hostWindow);
    Widget::trace(visitor);
}

void Scrollbar::setFrameRect(const IntRect& frameRect)
{
    if (frameRect == this->frameRect())
        return;

    Widget::setFrameRect(frameRect);
    setNeedsPaintInvalidation(AllParts);
}

ScrollbarOverlayStyle Scrollbar::getScrollbarOverlayStyle() const
{
    return m_scrollableArea ? m_scrollableArea->getScrollbarOverlayStyle() : ScrollbarOverlayStyleDefault;
}

void Scrollbar::getTickmarks(Vector<IntRect>& tickmarks) const
{
    if (m_scrollableArea)
        m_scrollableArea->getTickmarks(tickmarks);
}

bool Scrollbar::isScrollableAreaActive() const
{
    return m_scrollableArea && m_scrollableArea->isActive();
}

bool Scrollbar::isLeftSideVerticalScrollbar() const
{
    if (m_orientation == VerticalScrollbar && m_scrollableArea)
        return m_scrollableArea->shouldPlaceVerticalScrollbarOnLeft();
    return false;
}

void Scrollbar::offsetDidChange()
{
    ASSERT(m_scrollableArea);

    float position = scrollableAreaCurrentPos();
    if (position == m_currentPos)
        return;

    float oldPosition = m_currentPos;
    int oldThumbPosition = theme().thumbPosition(*this);
    m_currentPos = position;

    ScrollbarPart invalidParts = theme().invalidateOnThumbPositionChange(
        *this, oldPosition, position);
    setNeedsPaintInvalidation(invalidParts);

    if (m_pressedPart == ThumbPart)
        setPressedPos(m_pressedPos + theme().thumbPosition(*this) - oldThumbPosition);
}

void Scrollbar::disconnectFromScrollableArea()
{
    m_scrollableArea = nullptr;
}

void Scrollbar::setProportion(int visibleSize, int totalSize)
{
    if (visibleSize == m_visibleSize && totalSize == m_totalSize)
        return;

    m_visibleSize = visibleSize;
    m_totalSize = totalSize;

    setNeedsPaintInvalidation(AllParts);
}

void Scrollbar::paint(GraphicsContext& context, const CullRect& cullRect) const
{
    if (!cullRect.intersectsCullRect(frameRect()))
        return;

    if (!theme().paint(*this, context, cullRect))
        Widget::paint(context, cullRect);
}

void Scrollbar::autoscrollTimerFired(Timer<Scrollbar>*)
{
    autoscrollPressedPart(theme().autoscrollTimerDelay());
}

bool Scrollbar::thumbWillBeUnderMouse() const
{
    int thumbPos = theme().trackPosition(*this) + theme().thumbPosition(*this, scrollableAreaTargetPos());
    int thumbLength = theme().thumbLength(*this);
    return pressedPos() >= thumbPos && pressedPos() < thumbPos + thumbLength;
}

void Scrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbWillBeUnderMouse()) {
        setHoveredPart(ThumbPart);
        return;
    }

    // Handle the arrows and track.
    if (m_scrollableArea && m_scrollableArea->userScroll(pressedPartScrollGranularity(), toScrollDelta(pressedPartScrollDirectionPhysical(), 1)).didScroll())
        startTimerIfNeeded(delay);
}

void Scrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbWillBeUnderMouse()) {
        setHoveredPart(ThumbPart);
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirectionPhysical dir = pressedPartScrollDirectionPhysical();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == maximum())
            return;
    }

    m_scrollTimer.startOneShot(delay, BLINK_FROM_HERE);
}

void Scrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

ScrollDirectionPhysical Scrollbar::pressedPartScrollDirectionPhysical()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity Scrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart ||  m_pressedPart == ForwardButtonStartPart || m_pressedPart == ForwardButtonEndPart)
        return ScrollByLine;
    return ScrollByPage;
}

void Scrollbar::moveThumb(int pos, bool draggingDocument)
{
    if (!m_scrollableArea)
        return;

    int delta = pos - m_pressedPos;

    if (draggingDocument) {
        if (m_draggingDocument)
            delta = pos - m_documentDragPos;
        m_draggingDocument = true;
        FloatPoint currentPosition = m_scrollableArea->scrollAnimator().currentPosition();
        float destinationPosition = (m_orientation == HorizontalScrollbar ? currentPosition.x() : currentPosition.y()) + delta;
        destinationPosition = m_scrollableArea->clampScrollPosition(m_orientation, destinationPosition);
        m_scrollableArea->setScrollPositionSingleAxis(m_orientation, destinationPosition, UserScroll);
        m_documentDragPos = pos;
        return;
    }

    if (m_draggingDocument) {
        delta += m_pressedPos - m_documentDragPos;
        m_draggingDocument = false;
    }

    // Drag the thumb.
    int thumbPos = theme().thumbPosition(*this);
    int thumbLen = theme().thumbLength(*this);
    int trackLen = theme().trackLength(*this);
    ASSERT(thumbLen <= trackLen);
    if (thumbLen == trackLen)
        return;

    if (delta > 0)
        delta = std::min(trackLen - thumbLen - thumbPos, delta);
    else if (delta < 0)
        delta = std::max(-thumbPos, delta);

    float minPos = m_scrollableArea->minimumScrollPosition(m_orientation);
    float maxPos = m_scrollableArea->maximumScrollPosition(m_orientation);
    if (delta) {
        float newPosition = static_cast<float>(thumbPos + delta) * (maxPos - minPos) / (trackLen - thumbLen) + minPos;
        m_scrollableArea->setScrollPositionSingleAxis(m_orientation, newPosition, UserScroll);
    }
}

void Scrollbar::setHoveredPart(ScrollbarPart part)
{
    if (part == m_hoveredPart)
        return;

    if (((m_hoveredPart == NoPart || part == NoPart) && theme().invalidateOnMouseEnterExit())
        // When there's a pressed part, we don't draw a hovered state, so there's no reason to invalidate.
        || m_pressedPart == NoPart)
        setNeedsPaintInvalidation(static_cast<ScrollbarPart>(m_hoveredPart | part));

    m_hoveredPart = part;
}

void Scrollbar::setPressedPart(ScrollbarPart part)
{
    if (m_pressedPart != NoPart
        // When we no longer have a pressed part, we can start drawing a hovered state on the hovered part.
        || m_hoveredPart != NoPart)
        setNeedsPaintInvalidation(static_cast<ScrollbarPart>(m_pressedPart | m_hoveredPart | part));
    m_pressedPart = part;
}

bool Scrollbar::gestureEvent(const PlatformGestureEvent& evt, bool* shouldUpdateCapture)
{
    DCHECK(shouldUpdateCapture);
    switch (evt.type()) {
    case PlatformEvent::GestureTapDown:
        setPressedPart(theme().hitTest(*this, evt.position()));
        m_pressedPos = orientation() == HorizontalScrollbar ? convertFromRootFrame(evt.position()).x() : convertFromRootFrame(evt.position()).y();
        *shouldUpdateCapture = true;
        return true;
    case PlatformEvent::GestureTapDownCancel:
        if (m_pressedPart != ThumbPart)
            return false;
        m_scrollPos = m_pressedPos;
        return true;
    case PlatformEvent::GestureScrollBegin:
        switch (evt.source()) {
        case PlatformGestureSourceTouchpad:
            // Update the state on GSB for touchpad since GestureTapDown
            // is not generated by that device. Touchscreen uses the tap down
            // gesture since the scrollbar enters a visual active state.
            *shouldUpdateCapture = true;
            setPressedPart(NoPart);
            m_pressedPos = 0;
            return true;
        case PlatformGestureSourceTouchscreen:
            if (m_pressedPart != ThumbPart)
                return false;
            m_scrollPos = m_pressedPos;
            return true;
        default:
            ASSERT_NOT_REACHED();
            return true;
        }
        break;
    case PlatformEvent::GestureScrollUpdate:
        switch (evt.source()) {
        case PlatformGestureSourceTouchpad: {
            FloatSize delta(-evt.deltaX(), -evt.deltaY());
            if (m_scrollableArea && m_scrollableArea->userScroll(evt.deltaUnits(), delta).didScroll()) {
                return true;
            }
            return false;
        }
        case PlatformGestureSourceTouchscreen:
            if (m_pressedPart != ThumbPart)
                return false;
            m_scrollPos += orientation() == HorizontalScrollbar ? evt.deltaX() : evt.deltaY();
            moveThumb(m_scrollPos, false);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return true;
        }
        break;
    case PlatformEvent::GestureScrollEnd:
    case PlatformEvent::GestureLongPress:
    case PlatformEvent::GestureFlingStart:
        m_scrollPos = 0;
        m_pressedPos = 0;
        setPressedPart(NoPart);
        return false;
    case PlatformEvent::GestureTap: {
        if (m_pressedPart != ThumbPart && m_pressedPart != NoPart && m_scrollableArea
            && m_scrollableArea->userScroll(pressedPartScrollGranularity(), toScrollDelta(pressedPartScrollDirectionPhysical(), 1)).didScroll()) {
            return true;
        }
        m_scrollPos = 0;
        m_pressedPos = 0;
        setPressedPart(NoPart);
        return false;
    }
    default:
        // By default, we assume that gestures don't deselect the scrollbar.
        return true;
    }
}

void Scrollbar::mouseMoved(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) {
        if (theme().shouldSnapBackToDragOrigin(*this, evt)) {
            if (m_scrollableArea) {
                m_scrollableArea->setScrollPositionSingleAxis(m_orientation, m_dragOrigin + m_scrollableArea->minimumScrollPosition(m_orientation), UserScroll);
            }
        } else {
            moveThumb(m_orientation == HorizontalScrollbar
                ? convertFromRootFrame(evt.position()).x()
                : convertFromRootFrame(evt.position()).y(), theme().shouldDragDocumentInsteadOfThumb(*this, evt));
        }
        return;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = orientation() == HorizontalScrollbar ? convertFromRootFrame(evt.position()).x() : convertFromRootFrame(evt.position()).y();

    ScrollbarPart part = theme().hitTest(*this, evt.position());
    if (part != m_hoveredPart) {
        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(theme().autoscrollTimerDelay());
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
            }
        }

        setHoveredPart(part);
    }

    return;
}

void Scrollbar::mouseEntered()
{
    if (m_scrollableArea)
        m_scrollableArea->mouseEnteredScrollbar(*this);
}

void Scrollbar::mouseExited()
{
    if (m_scrollableArea)
        m_scrollableArea->mouseExitedScrollbar(*this);
    setHoveredPart(NoPart);
}

void Scrollbar::mouseUp(const PlatformMouseEvent& mouseEvent)
{
    setPressedPart(NoPart);
    m_pressedPos = 0;
    m_draggingDocument = false;
    stopTimerIfNeeded();

    if (m_scrollableArea) {
        // m_hoveredPart won't be updated until the next mouseMoved or mouseDown, so we have to hit test
        // to really know if the mouse has exited the scrollbar on a mouseUp.
        ScrollbarPart part = theme().hitTest(*this, mouseEvent.position());
        if (part == NoPart)
            m_scrollableArea->mouseExitedScrollbar(*this);
    }
}

void Scrollbar::mouseDown(const PlatformMouseEvent& evt)
{
    // Early exit for right click
    if (evt.button() == RightButton)
        return;

    setPressedPart(theme().hitTest(*this, evt.position()));
    int pressedPos = orientation() == HorizontalScrollbar ? convertFromRootFrame(evt.position()).x() : convertFromRootFrame(evt.position()).y();

    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && theme().shouldCenterOnThumb(*this, evt)) {
        setHoveredPart(ThumbPart);
        setPressedPart(ThumbPart);
        m_dragOrigin = m_currentPos;
        int thumbLen = theme().thumbLength(*this);
        int desiredPos = pressedPos;
        // Set the pressed position to the middle of the thumb so that when we do the move, the delta
        // will be from the current pixel position of the thumb to the new desired position for the thumb.
        m_pressedPos = theme().trackPosition(*this) + theme().thumbPosition(*this) + thumbLen / 2;
        moveThumb(desiredPos);
        return;
    }
    if (m_pressedPart == ThumbPart)
        m_dragOrigin = m_currentPos;

    m_pressedPos = pressedPos;

    autoscrollPressedPart(theme().initialAutoscrollTimerDelay());
}

void Scrollbar::visibilityChanged()
{
    if (m_scrollableArea)
        m_scrollableArea->scrollbarVisibilityChanged();
}

void Scrollbar::setEnabled(bool e)
{
    if (m_enabled == e)
        return;
    m_enabled = e;
    theme().updateEnabledState(*this);
    setNeedsPaintInvalidation(AllParts);
}

int Scrollbar::scrollbarThickness() const
{
    int thickness = orientation() == HorizontalScrollbar ? height() : width();
    if (!thickness || !m_hostWindow)
        return thickness;
    return m_hostWindow->windowToViewportScalar(m_theme.scrollbarThickness(controlSize()));
}


bool Scrollbar::isOverlayScrollbar() const
{
    return m_theme.usesOverlayScrollbars();
}

bool Scrollbar::shouldParticipateInHitTesting()
{
    // Non-overlay scrollbars should always participate in hit testing.
    if (!isOverlayScrollbar())
        return true;
    return m_scrollableArea->scrollAnimator().shouldScrollbarParticipateInHitTesting(*this);
}

bool Scrollbar::isWindowActive() const
{
    return m_scrollableArea && m_scrollableArea->isActive();
}

IntRect Scrollbar::convertToContainingWidget(const IntRect& localRect) const
{
    if (m_scrollableArea)
        return m_scrollableArea->convertFromScrollbarToContainingWidget(*this, localRect);

    return Widget::convertToContainingWidget(localRect);
}

IntRect Scrollbar::convertFromContainingWidget(const IntRect& parentRect) const
{
    if (m_scrollableArea)
        return m_scrollableArea->convertFromContainingWidgetToScrollbar(*this, parentRect);

    return Widget::convertFromContainingWidget(parentRect);
}

IntPoint Scrollbar::convertToContainingWidget(const IntPoint& localPoint) const
{
    if (m_scrollableArea)
        return m_scrollableArea->convertFromScrollbarToContainingWidget(*this, localPoint);

    return Widget::convertToContainingWidget(localPoint);
}

IntPoint Scrollbar::convertFromContainingWidget(const IntPoint& parentPoint) const
{
    if (m_scrollableArea)
        return m_scrollableArea->convertFromContainingWidgetToScrollbar(*this, parentPoint);

    return Widget::convertFromContainingWidget(parentPoint);
}

float Scrollbar::scrollableAreaCurrentPos() const
{
    if (!m_scrollableArea)
        return 0;

    if (m_orientation == HorizontalScrollbar)
        return m_scrollableArea->scrollPosition().x() - m_scrollableArea->minimumScrollPosition().x();

    return m_scrollableArea->scrollPosition().y() - m_scrollableArea->minimumScrollPosition().y();
}

float Scrollbar::scrollableAreaTargetPos() const
{
    if (!m_scrollableArea)
        return 0;

    if (m_orientation == HorizontalScrollbar)
        return m_scrollableArea->scrollAnimator().desiredTargetPosition().x() - m_scrollableArea->minimumScrollPosition().x();

    return m_scrollableArea->scrollAnimator().desiredTargetPosition().y() - m_scrollableArea->minimumScrollPosition().y();
}

LayoutRect Scrollbar::visualRect() const
{
    return m_scrollableArea ? m_scrollableArea->visualRectForScrollbarParts() : LayoutRect();
}

void Scrollbar::setNeedsPaintInvalidation(ScrollbarPart invalidParts)
{
    if (m_theme.shouldRepaintAllPartsOnInvalidation())
        invalidParts = AllParts;
    if (invalidParts & ~ThumbPart)
        m_trackNeedsRepaint = true;
    if (invalidParts & ThumbPart)
        m_thumbNeedsRepaint = true;
    if (m_scrollableArea)
        m_scrollableArea->setScrollbarNeedsPaintInvalidation(orientation());
}

} // namespace blink
