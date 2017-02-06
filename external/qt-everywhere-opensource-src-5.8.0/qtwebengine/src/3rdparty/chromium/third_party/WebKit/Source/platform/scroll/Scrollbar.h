/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef Scrollbar_h
#define Scrollbar_h

#include "platform/Timer.h"
#include "platform/Widget.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "wtf/MathExtras.h"

namespace blink {

class GraphicsContext;
class HostWindow;
class IntRect;
class PlatformGestureEvent;
class PlatformMouseEvent;
class ScrollAnimatorBase;
class ScrollableArea;
class ScrollbarTheme;

class PLATFORM_EXPORT Scrollbar : public Widget, public ScrollbarThemeClient, public DisplayItemClient {
public:
    static Scrollbar* create(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize size, HostWindow* hostWindow)
    {
        return new Scrollbar(scrollableArea, orientation, size, hostWindow);
    }

    // Theme object ownership remains with the caller and it must outlive the scrollbar.
    static Scrollbar* createForTesting(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize size, ScrollbarTheme* theme)
    {
        return new Scrollbar(scrollableArea, orientation, size, nullptr, theme);
    }

    ~Scrollbar() override;

    // ScrollbarThemeClient implementation.
    int x() const override { return Widget::x(); }
    int y() const override { return Widget::y(); }
    int width() const override { return Widget::width(); }
    int height() const override { return Widget::height(); }
    IntSize size() const override { return Widget::size(); }
    IntPoint location() const override { return Widget::location(); }

    Widget* parent() const override { return Widget::parent(); }
    Widget* root() const override { return Widget::root(); }

    void setFrameRect(const IntRect&) override;
    IntRect frameRect() const override { return Widget::frameRect(); }

    ScrollbarOverlayStyle getScrollbarOverlayStyle() const override;
    void getTickmarks(Vector<IntRect>&) const override;
    bool isScrollableAreaActive() const override;

    IntPoint convertFromRootFrame(const IntPoint& pointInRootFrame) const override { return Widget::convertFromRootFrame(pointInRootFrame); }

    bool isCustomScrollbar() const override { return false; }
    ScrollbarOrientation orientation() const override { return m_orientation; }
    bool isLeftSideVerticalScrollbar() const override;

    int value() const override { return lroundf(m_currentPos); }
    float currentPos() const override { return m_currentPos; }
    int visibleSize() const override { return m_visibleSize; }
    int totalSize() const override { return m_totalSize; }
    int maximum() const override { return m_totalSize - m_visibleSize; }
    ScrollbarControlSize controlSize() const override { return m_controlSize; }

    ScrollbarPart pressedPart() const override { return m_pressedPart; }
    ScrollbarPart hoveredPart() const override { return m_hoveredPart; }

    void styleChanged() override { }
    void visibilityChanged() override;
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool) override;

    int scrollbarThickness() const;

    // Called by the ScrollableArea when the scroll offset changes.
    // Will trigger paint invalidation if required.
    void offsetDidChange();

    void disconnectFromScrollableArea();
    ScrollableArea* getScrollableArea() const { return m_scrollableArea; }

    int pressedPos() const { return m_pressedPos; }

    virtual void setHoveredPart(ScrollbarPart);
    virtual void setPressedPart(ScrollbarPart);

    void setProportion(int visibleSize, int totalSize);
    void setPressedPos(int p) { m_pressedPos = p; }

    void paint(GraphicsContext&, const CullRect&) const final;

    bool isOverlayScrollbar() const override;
    bool shouldParticipateInHitTesting();

    bool isWindowActive() const;

    // Return if the gesture event was handled. |shouldUpdateCapture|
    // will be set to true if the handler should update the capture
    // state for this scrollbar.
    bool gestureEvent(const PlatformGestureEvent&, bool* shouldUpdateCapture);

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
    void mouseMoved(const PlatformMouseEvent&);
    void mouseEntered();
    void mouseExited();

    // Used by some platform scrollbars to know when they've been released from capture.
    void mouseUp(const PlatformMouseEvent&);
    void mouseDown(const PlatformMouseEvent&);

    ScrollbarTheme& theme() const { return m_theme; }

    IntRect convertToContainingWidget(const IntRect&) const override;
    IntRect convertFromContainingWidget(const IntRect&) const override;

    IntPoint convertToContainingWidget(const IntPoint&) const override;
    IntPoint convertFromContainingWidget(const IntPoint&) const override;

    void moveThumb(int pos, bool draggingDocument = false);

    float elasticOverscroll() const override { return m_elasticOverscroll; }
    void setElasticOverscroll(float elasticOverscroll) override { m_elasticOverscroll = elasticOverscroll; }

    // Use setNeedsPaintInvalidation to cause the scrollbar (or parts thereof)
    // to repaint.
    bool trackNeedsRepaint() const { return m_trackNeedsRepaint; }
    void clearTrackNeedsRepaint() { m_trackNeedsRepaint = false; }
    bool thumbNeedsRepaint() const { return m_thumbNeedsRepaint; }
    void clearThumbNeedsRepaint() { m_thumbNeedsRepaint = false; }

    bool overlapsResizer() const { return m_overlapsResizer; }
    void setOverlapsResizer(bool overlapsResizer) { m_overlapsResizer = overlapsResizer; }

    // DisplayItemClient methods.
    String debugName() const final { return m_orientation == HorizontalScrollbar ? "HorizontalScrollbar" : "VerticalScrollbar"; }
    LayoutRect visualRect() const override;

    // Marks the scrollbar as needing to be redrawn.
    //
    // If invalid parts are provided, then those parts will also be repainted.
    // Otherwise, the ScrollableArea may redraw using cached renderings of
    // individual parts. For instance, if the scrollbar is composited, the thumb
    // may be cached in a GPU texture (and is only guaranteed to be repainted if
    // ThumbPart is invalidated).
    //
    // Even if no parts are invalidated, the scrollbar may need to be redrawn
    // if, for instance, the thumb moves without changing the appearance of any
    // part.
    void setNeedsPaintInvalidation(ScrollbarPart invalidParts);

    // Promptly unregister from the theme manager + run finalizers of derived Scrollbars.
    EAGERLY_FINALIZE();
    DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();
    DECLARE_VIRTUAL_TRACE();

protected:
    Scrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize, HostWindow* = 0, ScrollbarTheme* = 0);

    void autoscrollTimerFired(Timer<Scrollbar>*);
    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirectionPhysical pressedPartScrollDirectionPhysical();
    ScrollGranularity pressedPartScrollGranularity();

    Member<ScrollableArea> m_scrollableArea;
    ScrollbarOrientation m_orientation;
    ScrollbarControlSize m_controlSize;
    ScrollbarTheme& m_theme;
    Member<HostWindow> m_hostWindow;

    int m_visibleSize;
    int m_totalSize;
    float m_currentPos;
    float m_dragOrigin;

    ScrollbarPart m_hoveredPart;
    ScrollbarPart m_pressedPart;
    int m_pressedPos;
    float m_scrollPos;
    bool m_draggingDocument;
    int m_documentDragPos;

    bool m_enabled;

    Timer<Scrollbar> m_scrollTimer;
    bool m_overlapsResizer;

    float m_elasticOverscroll;

private:
    bool isScrollbar() const override { return true; }

    void invalidate() override { setNeedsPaintInvalidation(AllParts); }
    void invalidateRect(const IntRect&) override { setNeedsPaintInvalidation(AllParts); }

    float scrollableAreaCurrentPos() const;
    float scrollableAreaTargetPos() const;
    bool thumbWillBeUnderMouse() const;

    bool m_trackNeedsRepaint;
    bool m_thumbNeedsRepaint;
};

DEFINE_TYPE_CASTS(Scrollbar, Widget, widget, widget->isScrollbar(), widget.isScrollbar());

} // namespace blink

#endif // Scrollbar_h
