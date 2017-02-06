/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/WebInputEventConversion.h"

#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/KeyboardCodes.h"
#include "platform/Widget.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {
float scaleDeltaToWindow(const Widget* widget, float delta)
{
    float scale = 1;
    if (widget) {
        FrameView* rootView = toFrameView(widget->root());
        if (rootView)
            scale = rootView->inputEventsScaleFactor();
    }
    return delta / scale;
}

FloatSize scaleSizeToWindow(const Widget* widget, FloatSize size)
{
    return FloatSize(scaleDeltaToWindow(widget, size.width()), scaleDeltaToWindow(widget, size.height()));
}

// This method converts from the renderer's coordinate space into Blink's root frame coordinate space.
// It's somewhat unique in that it takes into account DevTools emulation, which applies a scale and offset
// in the root layer (see updateRootLayerTransform in WebViewImpl) as well as the overscroll effect on OSX.
// This is in addition to the visual viewport "pinch-zoom" transformation and is one of the few cases where
// the visual viewport is not equal to the renderer's coordinate-space.
FloatPoint convertHitPointToRootFrame(const Widget* widget, FloatPoint pointInRendererViewport)
{
    float scale = 1;
    IntSize offset;
    IntPoint visualViewport;
    FloatSize overscrollOffset;
    if (widget) {
        FrameView* rootView = toFrameView(widget->root());
        if (rootView) {
            scale = rootView->inputEventsScaleFactor();
            offset = rootView->inputEventsOffsetForEmulation();
            visualViewport = flooredIntPoint(rootView->page()->frameHost().visualViewport().visibleRect().location());
            overscrollOffset = rootView->page()->frameHost().chromeClient().elasticOverscroll();
        }
    }
    return FloatPoint(
        (pointInRendererViewport.x() - offset.width()) / scale + visualViewport.x() + overscrollOffset.width(),
        (pointInRendererViewport.y() - offset.height()) / scale + visualViewport.y() + overscrollOffset.height());
}

PlatformEvent::DispatchType toPlatformDispatchType(WebInputEvent::DispatchType type)
{
    static_assert(PlatformEvent::DispatchType::Blocking == static_cast<PlatformEvent::DispatchType>(WebInputEvent::DispatchType::Blocking),
        "Dispatch Types not equal");
    static_assert(PlatformEvent::DispatchType::EventNonBlocking == static_cast<PlatformEvent::DispatchType>(WebInputEvent::DispatchType::EventNonBlocking),
        "Dispatch Types not equal");
    static_assert(PlatformEvent::DispatchType::ListenersNonBlockingPassive == static_cast<PlatformEvent::DispatchType>(WebInputEvent::DispatchType::ListenersNonBlockingPassive),
        "Dispatch Types not equal");
    static_assert(PlatformEvent::DispatchType::ListenersForcedNonBlockingPassive == static_cast<PlatformEvent::DispatchType>(WebInputEvent::DispatchType::ListenersForcedNonBlockingPassive),
        "Dispatch Types not equal");

    return static_cast<PlatformEvent::DispatchType>(type);
}

unsigned toPlatformModifierFrom(WebMouseEvent::Button button)
{
    if (button == WebMouseEvent::ButtonNone)
        return 0;

    unsigned webMouseButtonToPlatformModifier[] = {
        PlatformEvent::LeftButtonDown,
        PlatformEvent::MiddleButtonDown,
        PlatformEvent::RightButtonDown
    };

    return webMouseButtonToPlatformModifier[button];
}

ScrollGranularity toPlatformScrollGranularity(WebGestureEvent::ScrollUnits units)
{
    switch (units) {
    case WebGestureEvent::ScrollUnits::PrecisePixels:
        return ScrollGranularity::ScrollByPrecisePixel;
    case WebGestureEvent::ScrollUnits::Pixels:
        return ScrollGranularity::ScrollByPixel;
    case WebGestureEvent::ScrollUnits::Page:
        return ScrollGranularity::ScrollByPage;
    default:
        NOTREACHED();
        return ScrollGranularity::ScrollByPrecisePixel;
    }
}

ScrollInertialPhase toPlatformScrollInertialPhase(WebGestureEvent::InertialPhaseState state)
{
    static_assert(ScrollInertialPhaseUnknown == static_cast<ScrollInertialPhase>(WebGestureEvent::UnknownMomentumPhase),
        "Inertial phases not equal");
    static_assert(ScrollInertialPhaseNonMomentum == static_cast<ScrollInertialPhase>(WebGestureEvent::NonMomentumPhase),
        "Inertial phases not equal");
    static_assert(ScrollInertialPhaseMomentum == static_cast<ScrollInertialPhase>(WebGestureEvent::MomentumPhase),
        "Inertial phases not equal");

    return static_cast<ScrollInertialPhase>(state);
}

WebGestureEvent::InertialPhaseState toWebGestureInertialPhaseState(ScrollInertialPhase state)
{
    return static_cast<WebGestureEvent::InertialPhaseState>(state);
}

WebGestureEvent::ScrollUnits toWebGestureScrollUnits(ScrollGranularity granularity)
{
    switch (granularity) {
    case ScrollGranularity::ScrollByPrecisePixel:
        return WebGestureEvent::ScrollUnits::PrecisePixels;
    case ScrollGranularity::ScrollByPixel:
        return WebGestureEvent::ScrollUnits::Pixels;
    case ScrollGranularity::ScrollByPage:
        return WebGestureEvent::ScrollUnits::Page;
    default:
        NOTREACHED();
        return WebGestureEvent::ScrollUnits::PrecisePixels;
    }
}

} // namespace

// MakePlatformMouseEvent -----------------------------------------------------

// TODO(mustaq): Add tests for this.
PlatformMouseEventBuilder::PlatformMouseEventBuilder(Widget* widget, const WebMouseEvent& e)
{
    // FIXME: Widget is always toplevel, unless it's a popup. We may be able
    // to get rid of this once we abstract popups into a WebKit API.
    m_position = widget->convertFromRootFrame(flooredIntPoint(convertHitPointToRootFrame(widget, IntPoint(e.x, e.y))));
    m_globalPosition = IntPoint(e.globalX, e.globalY);
    m_movementDelta = IntPoint(scaleDeltaToWindow(widget, e.movementX), scaleDeltaToWindow(widget, e.movementY));
    m_button = static_cast<MouseButton>(e.button);
    m_modifiers = e.modifiers;

    m_timestamp = e.timeStampSeconds;
    m_clickCount = e.clickCount;

    m_pointerProperties = static_cast<WebPointerProperties>(e);

    switch (e.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter: // synthesize a move event
    case WebInputEvent::MouseLeave: // synthesize a move event
        m_type = PlatformEvent::MouseMoved;
        break;

    case WebInputEvent::MouseDown:
        m_type = PlatformEvent::MousePressed;
        break;

    case WebInputEvent::MouseUp:
        m_type = PlatformEvent::MouseReleased;

        // The MouseEvent spec requires that buttons indicates the state
        // immediately after the event takes place. To ensure consistency
        // between platforms here, we explicitly clear the button that is
        // in the process of being released.
        m_modifiers &= ~toPlatformModifierFrom(e.button);
        break;

    default:
        NOTREACHED();
    }
}

// PlatformWheelEventBuilder --------------------------------------------------

PlatformWheelEventBuilder::PlatformWheelEventBuilder(Widget* widget, const WebMouseWheelEvent& e)
{
    m_position = widget->convertFromRootFrame(flooredIntPoint(convertHitPointToRootFrame(widget, FloatPoint(e.x, e.y))));
    m_globalPosition = IntPoint(e.globalX, e.globalY);
    m_deltaX = scaleDeltaToWindow(widget, e.deltaX);
    m_deltaY = scaleDeltaToWindow(widget, e.deltaY);
    m_wheelTicksX = e.wheelTicksX;
    m_wheelTicksY = e.wheelTicksY;
    m_granularity = e.scrollByPage ?
        ScrollByPageWheelEvent : ScrollByPixelWheelEvent;

    m_type = PlatformEvent::Wheel;

    m_timestamp = e.timeStampSeconds;
    m_modifiers = e.modifiers;
    m_dispatchType = toPlatformDispatchType(e.dispatchType);

    m_hasPreciseScrollingDeltas = e.hasPreciseScrollingDeltas;
    m_resendingPluginId = e.resendingPluginId;
    m_railsMode = static_cast<PlatformEvent::RailsMode>(e.railsMode);
#if OS(MACOSX)
    m_phase = static_cast<PlatformWheelEventPhase>(e.phase);
    m_momentumPhase = static_cast<PlatformWheelEventPhase>(e.momentumPhase);
    m_canRubberbandLeft = e.canRubberbandLeft;
    m_canRubberbandRight = e.canRubberbandRight;
#endif
}

// PlatformGestureEventBuilder --------------------------------------------------

PlatformGestureEventBuilder::PlatformGestureEventBuilder(Widget* widget, const WebGestureEvent& e)
{
    switch (e.type) {
    case WebInputEvent::GestureScrollBegin:
        m_type = PlatformEvent::GestureScrollBegin;
        m_data.m_scroll.m_resendingPluginId = e.resendingPluginId;
        m_data.m_scroll.m_deltaX = e.data.scrollBegin.deltaXHint;
        m_data.m_scroll.m_deltaY = e.data.scrollBegin.deltaYHint;
        m_data.m_scroll.m_deltaUnits = toPlatformScrollGranularity(e.data.scrollBegin.deltaHintUnits);
        m_data.m_scroll.m_inertialPhase = toPlatformScrollInertialPhase(e.data.scrollBegin.inertialPhase);
        m_data.m_scroll.m_synthetic = e.data.scrollBegin.synthetic;
        break;
    case WebInputEvent::GestureScrollEnd:
        m_type = PlatformEvent::GestureScrollEnd;
        m_data.m_scroll.m_resendingPluginId = e.resendingPluginId;
        m_data.m_scroll.m_deltaUnits = toPlatformScrollGranularity(e.data.scrollEnd.deltaUnits);
        m_data.m_scroll.m_inertialPhase = toPlatformScrollInertialPhase(e.data.scrollEnd.inertialPhase);
        m_data.m_scroll.m_synthetic = e.data.scrollEnd.synthetic;
        break;
    case WebInputEvent::GestureFlingStart:
        m_type = PlatformEvent::GestureFlingStart;
        m_data.m_scroll.m_velocityX = e.data.flingStart.velocityX;
        m_data.m_scroll.m_velocityY = e.data.flingStart.velocityY;
        break;
    case WebInputEvent::GestureScrollUpdate:
        m_type = PlatformEvent::GestureScrollUpdate;
        m_data.m_scroll.m_resendingPluginId = e.resendingPluginId;
        m_data.m_scroll.m_deltaX = scaleDeltaToWindow(widget, e.data.scrollUpdate.deltaX);
        m_data.m_scroll.m_deltaY = scaleDeltaToWindow(widget, e.data.scrollUpdate.deltaY);
        m_data.m_scroll.m_velocityX = e.data.scrollUpdate.velocityX;
        m_data.m_scroll.m_velocityY = e.data.scrollUpdate.velocityY;
        m_data.m_scroll.m_preventPropagation = e.data.scrollUpdate.preventPropagation;
        m_data.m_scroll.m_inertialPhase = toPlatformScrollInertialPhase(e.data.scrollUpdate.inertialPhase);
        m_data.m_scroll.m_deltaUnits = toPlatformScrollGranularity(e.data.scrollUpdate.deltaUnits);
        break;
    case WebInputEvent::GestureTap:
        m_type = PlatformEvent::GestureTap;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.tap.width, e.data.tap.height)));
        m_data.m_tap.m_tapCount = e.data.tap.tapCount;
        break;
    case WebInputEvent::GestureTapUnconfirmed:
        m_type = PlatformEvent::GestureTapUnconfirmed;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.tap.width, e.data.tap.height)));
        break;
    case WebInputEvent::GestureTapDown:
        m_type = PlatformEvent::GestureTapDown;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.tapDown.width, e.data.tapDown.height)));
        break;
    case WebInputEvent::GestureShowPress:
        m_type = PlatformEvent::GestureShowPress;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.showPress.width, e.data.showPress.height)));
        break;
    case WebInputEvent::GestureTapCancel:
        m_type = PlatformEvent::GestureTapDownCancel;
        break;
    case WebInputEvent::GestureDoubleTap:
        // DoubleTap gesture is now handled as PlatformEvent::GestureTap with tap_count = 2. So no
        // need to convert to a Platfrom DoubleTap gesture. But in WebViewImpl::handleGestureEvent
        // all WebGestureEvent are converted to PlatformGestureEvent, for completeness and not reach
        // the NOTREACHED() at the end, convert the DoubleTap to a NoType.
        m_type = PlatformEvent::NoType;
        break;
    case WebInputEvent::GestureTwoFingerTap:
        m_type = PlatformEvent::GestureTwoFingerTap;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.twoFingerTap.firstFingerWidth, e.data.twoFingerTap.firstFingerHeight)));
        break;
    case WebInputEvent::GestureLongPress:
        m_type = PlatformEvent::GestureLongPress;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.longPress.width, e.data.longPress.height)));
        break;
    case WebInputEvent::GestureLongTap:
        m_type = PlatformEvent::GestureLongTap;
        m_area = expandedIntSize(scaleSizeToWindow(widget, FloatSize(e.data.longPress.width, e.data.longPress.height)));
        break;
    case WebInputEvent::GesturePinchBegin:
        m_type = PlatformEvent::GesturePinchBegin;
        break;
    case WebInputEvent::GesturePinchEnd:
        m_type = PlatformEvent::GesturePinchEnd;
        break;
    case WebInputEvent::GesturePinchUpdate:
        m_type = PlatformEvent::GesturePinchUpdate;
        m_data.m_pinchUpdate.m_scale = e.data.pinchUpdate.scale;
        break;
    default:
        NOTREACHED();
    }
    m_position = widget->convertFromRootFrame(flooredIntPoint(convertHitPointToRootFrame(widget, FloatPoint(e.x, e.y))));
    m_globalPosition = IntPoint(e.globalX, e.globalY);
    m_timestamp = e.timeStampSeconds;
    m_modifiers = e.modifiers;
    switch (e.sourceDevice) {
    case WebGestureDeviceTouchpad:
        m_source = PlatformGestureSourceTouchpad;
        break;
    case WebGestureDeviceTouchscreen:
        m_source = PlatformGestureSourceTouchscreen;
        break;
    case WebGestureDeviceUninitialized:
        NOTREACHED();
    }

    m_uniqueTouchEventId = e.uniqueTouchEventId;
}

// MakePlatformKeyboardEvent --------------------------------------------------

inline PlatformEvent::EventType toPlatformKeyboardEventType(WebInputEvent::Type type)
{
    switch (type) {
    case WebInputEvent::KeyUp:
        return PlatformEvent::KeyUp;
    case WebInputEvent::KeyDown:
        return PlatformEvent::KeyDown;
    case WebInputEvent::RawKeyDown:
        return PlatformEvent::RawKeyDown;
    case WebInputEvent::Char:
        return PlatformEvent::Char;
    default:
        NOTREACHED();
    }
    return PlatformEvent::KeyDown;
}

PlatformKeyboardEventBuilder::PlatformKeyboardEventBuilder(const WebKeyboardEvent& e)
{
    m_type = toPlatformKeyboardEventType(e.type);
    m_text = String(e.text);
    m_unmodifiedText = String(e.unmodifiedText);
    m_keyIdentifier = String(e.keyIdentifier);
    m_nativeVirtualKeyCode = e.nativeKeyCode;
    m_isSystemKey = e.isSystemKey;
    // TODO: BUG482880 Fix this initialization to lazy initialization.
    m_code = Platform::current()->domCodeStringFromEnum(e.domCode);
    m_key = Platform::current()->domKeyStringFromEnum(e.domKey);

    m_modifiers = e.modifiers;
    m_timestamp = e.timeStampSeconds;
    m_windowsVirtualKeyCode = e.windowsKeyCode;
}

void PlatformKeyboardEventBuilder::setKeyType(EventType type)
{
    // According to the behavior of Webkit in Windows platform,
    // we need to convert KeyDown to RawKeydown and Char events
    // See WebKit/WebKit/Win/WebView.cpp
    DCHECK(m_type == KeyDown);
    DCHECK(type == RawKeyDown || type == Char);
    m_type = type;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

// Please refer to bug http://b/issue?id=961192, which talks about Webkit
// keyboard event handling changes. It also mentions the list of keys
// which don't have associated character events.
bool PlatformKeyboardEventBuilder::isCharacterKey() const
{
    switch (windowsVirtualKeyCode()) {
    case VKEY_BACK:
    case VKEY_ESCAPE:
        return false;
    }
    return true;
}

inline PlatformEvent::EventType toPlatformTouchEventType(const WebInputEvent::Type type)
{
    switch (type) {
    case WebInputEvent::TouchStart:
        return PlatformEvent::TouchStart;
    case WebInputEvent::TouchMove:
        return PlatformEvent::TouchMove;
    case WebInputEvent::TouchEnd:
        return PlatformEvent::TouchEnd;
    case WebInputEvent::TouchCancel:
        return PlatformEvent::TouchCancel;
    case WebInputEvent::TouchScrollStarted:
        return PlatformEvent::TouchScrollStarted;
    default:
        NOTREACHED();
    }
    return PlatformEvent::TouchStart;
}

inline PlatformTouchPoint::TouchState toPlatformTouchPointState(const WebTouchPoint::State state)
{
    switch (state) {
    case WebTouchPoint::StateReleased:
        return PlatformTouchPoint::TouchReleased;
    case WebTouchPoint::StatePressed:
        return PlatformTouchPoint::TouchPressed;
    case WebTouchPoint::StateMoved:
        return PlatformTouchPoint::TouchMoved;
    case WebTouchPoint::StateStationary:
        return PlatformTouchPoint::TouchStationary;
    case WebTouchPoint::StateCancelled:
        return PlatformTouchPoint::TouchCancelled;
    case WebTouchPoint::StateUndefined:
        NOTREACHED();
    }
    return PlatformTouchPoint::TouchReleased;
}

inline WebTouchPoint::State toWebTouchPointState(const AtomicString& type)
{
    if (type == EventTypeNames::touchend)
        return WebTouchPoint::StateReleased;
    if (type == EventTypeNames::touchcancel)
        return WebTouchPoint::StateCancelled;
    if (type == EventTypeNames::touchstart)
        return WebTouchPoint::StatePressed;
    if (type == EventTypeNames::touchmove)
        return WebTouchPoint::StateMoved;
    return WebTouchPoint::StateUndefined;
}

// TODO(mustaq): Add tests for this.
PlatformTouchPointBuilder::PlatformTouchPointBuilder(Widget* widget, const WebTouchPoint& point)
{
    m_pointerProperties = point;
    m_state = toPlatformTouchPointState(point.state);

    FloatPoint floatPos = convertHitPointToRootFrame(widget, point.position);
    IntPoint flooredPoint = flooredIntPoint(floatPos);
    m_pos = widget->convertFromRootFrame(flooredPoint) + (floatPos - flooredPoint);

    m_screenPos = FloatPoint(point.screenPosition.x, point.screenPosition.y);
    m_radius = scaleSizeToWindow(widget, FloatSize(point.radiusX, point.radiusY));
    m_rotationAngle = point.rotationAngle;
}

PlatformTouchEventBuilder::PlatformTouchEventBuilder(Widget* widget, const WebTouchEvent& event)
{
    m_type = toPlatformTouchEventType(event.type);
    m_modifiers = event.modifiers;
    m_timestamp = event.timeStampSeconds;
    m_causesScrollingIfUncanceled = event.movedBeyondSlopRegion;
    m_dispatchedDuringFling = event.dispatchedDuringFling;

    for (unsigned i = 0; i < event.touchesLength; ++i)
        m_touchPoints.append(PlatformTouchPointBuilder(widget, event.touches[i]));

    m_dispatchType = toPlatformDispatchType(event.dispatchType);
    m_uniqueTouchEventId = event.uniqueTouchEventId;
}

static FloatPoint convertAbsoluteLocationForLayoutObjectFloat(const LayoutPoint& location, const LayoutItem layoutItem)
{
    return layoutItem.absoluteToLocal(FloatPoint(location), UseTransforms);
}

static IntPoint convertAbsoluteLocationForLayoutObject(const LayoutPoint& location, const LayoutItem layoutItem)
{
    return roundedIntPoint(convertAbsoluteLocationForLayoutObjectFloat(location, layoutItem));
}

// FIXME: Change |widget| to const Widget& after RemoteFrames get
// RemoteFrameViews.
static void updateWebMouseEventFromCoreMouseEvent(const MouseRelatedEvent& event, const Widget* widget, const LayoutItem layoutItem, WebMouseEvent& webEvent)
{
    webEvent.timeStampSeconds = event.platformTimeStamp();
    webEvent.modifiers = event.modifiers();

    FrameView* view = widget ? toFrameView(widget->parent()) : 0;
    // TODO(bokan): If view == nullptr, pointInRootFrame will really be pointInRootContent.
    IntPoint pointInRootFrame = IntPoint(event.absoluteLocation().x(), event.absoluteLocation().y());
    if (view)
        pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
    webEvent.globalX = event.screenX();
    webEvent.globalY = event.screenY();
    webEvent.windowX = pointInRootFrame.x();
    webEvent.windowY = pointInRootFrame.y();
    IntPoint localPoint = convertAbsoluteLocationForLayoutObject(event.absoluteLocation(), layoutItem);
    webEvent.x = localPoint.x();
    webEvent.y = localPoint.y();
}

WebMouseEventBuilder::WebMouseEventBuilder(const Widget* widget, const LayoutItem layoutItem, const MouseEvent& event)
{
    if (event.type() == EventTypeNames::mousemove)
        type = WebInputEvent::MouseMove;
    else if (event.type() == EventTypeNames::mouseout)
        type = WebInputEvent::MouseLeave;
    else if (event.type() == EventTypeNames::mouseover)
        type = WebInputEvent::MouseEnter;
    else if (event.type() == EventTypeNames::mousedown)
        type = WebInputEvent::MouseDown;
    else if (event.type() == EventTypeNames::mouseup)
        type = WebInputEvent::MouseUp;
    else if (event.type() == EventTypeNames::contextmenu)
        type = WebInputEvent::ContextMenu;
    else
        return; // Skip all other mouse events.

    updateWebMouseEventFromCoreMouseEvent(event, widget, layoutItem, *this);

    switch (event.button()) {
    case LeftButton:
        button = WebMouseEvent::ButtonLeft;
        break;
    case MiddleButton:
        button = WebMouseEvent::ButtonMiddle;
        break;
    case RightButton:
        button = WebMouseEvent::ButtonRight;
        break;
    }
    if (event.buttonDown()) {
        switch (event.button()) {
        case LeftButton:
            modifiers |= WebInputEvent::LeftButtonDown;
            break;
        case MiddleButton:
            modifiers |= WebInputEvent::MiddleButtonDown;
            break;
        case RightButton:
            modifiers |= WebInputEvent::RightButtonDown;
            break;
        }
    } else
        button = WebMouseEvent::ButtonNone;
    movementX = event.movementX();
    movementY = event.movementY();
    clickCount = event.detail();

    pointerType = WebPointerProperties::PointerType::Mouse;
}

// Generate a synthetic WebMouseEvent given a TouchEvent (eg. for emulating a mouse
// with touch input for plugins that don't support touch input).
WebMouseEventBuilder::WebMouseEventBuilder(const Widget* widget, const LayoutItem layoutItem, const TouchEvent& event)
{
    if (!event.touches())
        return;
    if (event.touches()->length() != 1) {
        if (event.touches()->length() || event.type() != EventTypeNames::touchend || !event.changedTouches() || event.changedTouches()->length() != 1)
            return;
    }

    const Touch* touch = event.touches()->length() == 1 ? event.touches()->item(0) : event.changedTouches()->item(0);
    if (touch->identifier())
        return;

    if (event.type() == EventTypeNames::touchstart)
        type = MouseDown;
    else if (event.type() == EventTypeNames::touchmove)
        type = MouseMove;
    else if (event.type() == EventTypeNames::touchend)
        type = MouseUp;
    else
        return;

    timeStampSeconds = event.platformTimeStamp();
    modifiers = event.modifiers();

    // The mouse event co-ordinates should be generated from the co-ordinates of the touch point.
    FrameView* view =  toFrameView(widget->parent());
    // FIXME: if view == nullptr, pointInRootFrame will really be pointInRootContent.
    IntPoint pointInRootFrame = roundedIntPoint(touch->absoluteLocation());
    if (view)
        pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
    IntPoint screenPoint = roundedIntPoint(touch->screenLocation());
    globalX = screenPoint.x();
    globalY = screenPoint.y();
    windowX = pointInRootFrame.x();
    windowY = pointInRootFrame.y();

    button = WebMouseEvent::ButtonLeft;
    modifiers |= WebInputEvent::LeftButtonDown;
    clickCount = (type == MouseDown || type == MouseUp);

    IntPoint localPoint = convertAbsoluteLocationForLayoutObject(touch->absoluteLocation(), layoutItem);
    x = localPoint.x();
    y = localPoint.y();

    pointerType = WebPointerProperties::PointerType::Touch;
}

WebMouseWheelEventBuilder::WebMouseWheelEventBuilder(const Widget* widget, const LayoutItem layoutItem, const WheelEvent& event)
{
    if (event.type() != EventTypeNames::wheel && event.type() != EventTypeNames::mousewheel)
        return;
    type = WebInputEvent::MouseWheel;
    updateWebMouseEventFromCoreMouseEvent(event, widget, layoutItem, *this);
    deltaX = -event.deltaX();
    deltaY = -event.deltaY();
    wheelTicksX = event.ticksX();
    wheelTicksY = event.ticksY();
    scrollByPage = event.deltaMode() == WheelEvent::DOM_DELTA_PAGE;
    resendingPluginId = event.resendingPluginId();
    railsMode = static_cast<RailsMode>(event.getRailsMode());
    hasPreciseScrollingDeltas = event.hasPreciseScrollingDeltas();
    dispatchType = event.cancelable() ? WebInputEvent::Blocking : WebInputEvent::EventNonBlocking;
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event)
{
    if (event.type() == EventTypeNames::keydown)
        type = KeyDown;
    else if (event.type() == EventTypeNames::keyup)
        type = WebInputEvent::KeyUp;
    else if (event.type() == EventTypeNames::keypress)
        type = WebInputEvent::Char;
    else
        return; // Skip all other keyboard events.

    modifiers = event.modifiers();

    timeStampSeconds = event.platformTimeStamp();
    windowsKeyCode = event.keyCode();

    // The platform keyevent does not exist if the event was created using
    // initKeyboardEvent.
    if (!event.keyEvent())
        return;
    nativeKeyCode = event.keyEvent()->nativeVirtualKeyCode();
    domCode = Platform::current()->domEnumFromCodeString(event.keyEvent()->code());
    domKey = Platform::current()->domKeyEnumFromString(event.keyEvent()->key());
    unsigned numberOfCharacters = std::min(event.keyEvent()->text().length(), static_cast<unsigned>(textLengthCap));
    for (unsigned i = 0; i < numberOfCharacters; ++i) {
        text[i] = event.keyEvent()->text()[i];
        unmodifiedText[i] = event.keyEvent()->unmodifiedText()[i];
    }
    memcpy(keyIdentifier, event.keyIdentifier().ascii().data(), event.keyIdentifier().length());
}

WebInputEvent::Type toWebKeyboardEventType(PlatformEvent::EventType type)
{
    switch (type) {
    case PlatformEvent::KeyUp:
        return WebInputEvent::KeyUp;
    case PlatformEvent::KeyDown:
        return WebInputEvent::KeyDown;
    case PlatformEvent::RawKeyDown:
        return WebInputEvent::RawKeyDown;
    case PlatformEvent::Char:
        return WebInputEvent::Char;
    default:
        return WebInputEvent::Undefined;
    }
}

static WebTouchPoint toWebTouchPoint(const Touch* touch, const LayoutItem layoutItem, WebTouchPoint::State state)
{
    WebTouchPoint point;
    point.pointerType = WebPointerProperties::PointerType::Touch;
    point.id = touch->identifier();
    point.screenPosition = touch->screenLocation();
    point.position = convertAbsoluteLocationForLayoutObjectFloat(touch->absoluteLocation(), layoutItem);
    point.radiusX = touch->radiusX();
    point.radiusY = touch->radiusY();
    point.rotationAngle = touch->rotationAngle();
    point.force = touch->force();
    point.state = state;
    return point;
}

static unsigned indexOfTouchPointWithId(const WebTouchPoint* touchPoints, unsigned touchPointsLength, unsigned id)
{
    for (unsigned i = 0; i < touchPointsLength; ++i) {
        if (touchPoints[i].id == static_cast<int>(id))
            return i;
    }
    return std::numeric_limits<unsigned>::max();
}

static void addTouchPointsUpdateStateIfNecessary(WebTouchPoint::State state, TouchList* touches, WebTouchPoint* touchPoints, unsigned* touchPointsLength, const LayoutItem layoutItem)
{
    unsigned initialTouchPointsLength = *touchPointsLength;
    for (unsigned i = 0; i < touches->length(); ++i) {
        const unsigned pointIndex = *touchPointsLength;
        if (pointIndex >= static_cast<unsigned>(WebTouchEvent::touchesLengthCap))
            return;

        const Touch* touch = touches->item(i);
        unsigned existingPointIndex = indexOfTouchPointWithId(touchPoints, initialTouchPointsLength, touch->identifier());
        if (existingPointIndex != std::numeric_limits<unsigned>::max()) {
            touchPoints[existingPointIndex].state = state;
        } else {
            touchPoints[pointIndex] = toWebTouchPoint(touch, layoutItem, state);
            ++(*touchPointsLength);
        }
    }
}

WebTouchEventBuilder::WebTouchEventBuilder(const LayoutItem layoutItem, const TouchEvent& event)
{
    if (event.type() == EventTypeNames::touchstart)
        type = TouchStart;
    else if (event.type() == EventTypeNames::touchmove)
        type = TouchMove;
    else if (event.type() == EventTypeNames::touchend)
        type = TouchEnd;
    else if (event.type() == EventTypeNames::touchcancel)
        type = TouchCancel;
    else {
        NOTREACHED();
        type = Undefined;
        return;
    }

    timeStampSeconds = event.platformTimeStamp();
    modifiers = event.modifiers();
    dispatchType = event.cancelable() ? WebInputEvent::Blocking : WebInputEvent::EventNonBlocking;
    movedBeyondSlopRegion = event.causesScrollingIfUncanceled();

    // Currently touches[] is empty, add stationary points as-is.
    for (unsigned i = 0; i < event.touches()->length() && i < static_cast<unsigned>(WebTouchEvent::touchesLengthCap); ++i) {
        touches[i] = toWebTouchPoint(event.touches()->item(i), layoutItem, WebTouchPoint::StateStationary);
        ++touchesLength;
    }
    // If any existing points are also in the change list, we should update
    // their state, otherwise just add the new points.
    addTouchPointsUpdateStateIfNecessary(toWebTouchPointState(event.type()), event.changedTouches(), touches, &touchesLength, layoutItem);
}

WebGestureEventBuilder::WebGestureEventBuilder(const LayoutItem layoutItem, const GestureEvent& event)
{
    if (event.type() == EventTypeNames::gestureshowpress) {
        type = GestureShowPress;
    } else if (event.type() == EventTypeNames::gesturelongpress) {
        type = GestureLongPress;
    } else if (event.type() == EventTypeNames::gesturetapdown) {
        type = GestureTapDown;
    } else if (event.type() == EventTypeNames::gesturescrollstart) {
        type = GestureScrollBegin;
        resendingPluginId = event.resendingPluginId();
        data.scrollBegin.deltaXHint = event.deltaX();
        data.scrollBegin.deltaYHint = event.deltaY();
        data.scrollBegin.deltaHintUnits = toWebGestureScrollUnits(event.deltaUnits());
        data.scrollBegin.inertialPhase = toWebGestureInertialPhaseState(event.inertialPhase());
        data.scrollBegin.synthetic = event.synthetic();
    } else if (event.type() == EventTypeNames::gesturescrollend) {
        type = GestureScrollEnd;
        resendingPluginId = event.resendingPluginId();
        data.scrollEnd.deltaUnits = toWebGestureScrollUnits(event.deltaUnits());
        data.scrollEnd.inertialPhase = toWebGestureInertialPhaseState(event.inertialPhase());
        data.scrollEnd.synthetic = event.synthetic();
    } else if (event.type() == EventTypeNames::gesturescrollupdate) {
        type = GestureScrollUpdate;
        data.scrollUpdate.deltaUnits = toWebGestureScrollUnits(event.deltaUnits());
        data.scrollUpdate.deltaX = event.deltaX();
        data.scrollUpdate.deltaY = event.deltaY();
        data.scrollUpdate.inertialPhase = toWebGestureInertialPhaseState(event.inertialPhase());
        resendingPluginId = event.resendingPluginId();
    } else if (event.type() == EventTypeNames::gestureflingstart) {
        type = GestureFlingStart;
        data.flingStart.velocityX = event.velocityX();
        data.flingStart.velocityY = event.velocityY();
    } else if (event.type() == EventTypeNames::gesturetap) {
        type = GestureTap;
        data.tap.tapCount = 1;
    }

    timeStampSeconds = event.platformTimeStamp();
    modifiers = event.modifiers();

    globalX = event.screenX();
    globalY = event.screenY();
    IntPoint localPoint = convertAbsoluteLocationForLayoutObject(event.absoluteLocation(), layoutItem);
    x = localPoint.x();
    y = localPoint.y();

    switch (event.source()) {
    case GestureSourceTouchpad:
        sourceDevice = WebGestureDeviceTouchpad;
        break;
    case GestureSourceTouchscreen:
        sourceDevice = WebGestureDeviceTouchscreen;
        break;
    case GestureSourceUninitialized:
        NOTREACHED();
    }
}

} // namespace blink
