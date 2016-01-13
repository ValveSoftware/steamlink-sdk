/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwlpointer_p.h"

#include "qwlcompositor_p.h"
#include "qwlinputdevice_p.h"
#include "qwlkeyboard_p.h"
#include "qwlsurface_p.h"
#include "qwaylandcompositor.h"
#include "qwaylandsurfaceview.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

using QtWaylandServer::wl_keyboard;

static uint32_t toWaylandButton(Qt::MouseButton button)
{
#ifndef BTN_LEFT
    uint32_t BTN_LEFT = 0x110;
#endif
    // the range of valid buttons (evdev module) is from 0x110
    // through 0x11f. 0x120 is the first 'Joystick' button.
    switch (button) {
    case Qt::LeftButton: return BTN_LEFT;
    case Qt::RightButton: return uint32_t(0x111);
    case Qt::MiddleButton: return uint32_t(0x112);
    case Qt::ExtraButton1: return uint32_t(0x113);  // AKA Qt::BackButton, Qt::XButton1
    case Qt::ExtraButton2: return uint32_t(0x114);  // AKA Qt::ForwardButton, Qt::XButton2
    case Qt::ExtraButton3: return uint32_t(0x115);
    case Qt::ExtraButton4: return uint32_t(0x116);
    case Qt::ExtraButton5: return uint32_t(0x117);
    case Qt::ExtraButton6: return uint32_t(0x118);
    case Qt::ExtraButton7: return uint32_t(0x119);
    case Qt::ExtraButton8: return uint32_t(0x11a);
    case Qt::ExtraButton9: return uint32_t(0x11b);
    case Qt::ExtraButton10: return uint32_t(0x11c);
    case Qt::ExtraButton11: return uint32_t(0x11d);
    case Qt::ExtraButton12: return uint32_t(0x11e);
    case Qt::ExtraButton13: return uint32_t(0x11f);
        // default should not occur; but if it does, then return Wayland's highest possible button number.
    default: return uint32_t(0x11f);
    }
}

Pointer::Pointer(Compositor *compositor, InputDevice *seat)
    : wl_pointer()
    , PointerGrabber()
    , m_compositor(compositor)
    , m_seat(seat)
    , m_grab(this)
    , m_grabButton()
    , m_grabTime()
    , m_grabSerial()
    , m_position(100, 100)
    , m_focus()
    , m_focusResource()
    , m_current()
    , m_currentPoint()
    , m_buttonCount()
{
    connect(&m_focusDestroyListener, &WlListener::fired, this, &Pointer::focusDestroyed);
}

void Pointer::setFocus(QWaylandSurfaceView *surface, const QPointF &position)
{
    if (m_focusResource && m_focus != surface) {
        uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
        send_leave(m_focusResource->handle, serial, m_focus->surface()->handle()->resource()->handle);
        m_focusDestroyListener.reset();
    }

    Resource *resource = surface ? resourceMap().value(surface->surface()->handle()->resource()->client()) : 0;

    if (resource && (m_focus != surface || resource != m_focusResource)) {
        uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
        Keyboard *keyboard = m_seat->keyboardDevice();
        if (keyboard) {
            wl_keyboard::Resource *kr = keyboard->resourceMap().value(surface->surface()->handle()->resource()->client());
            if (kr)
                keyboard->sendKeyModifiers(kr, serial);
        }
        send_enter(resource->handle, serial, surface->surface()->handle()->resource()->handle,
                   wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));

        m_focusDestroyListener.listenForDestruction(surface->surface()->handle()->resource()->handle);
    }

    m_focusResource = resource;
    m_focus = surface;
}

void Pointer::focusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_focusDestroyListener.reset();

    m_focus = 0;
    m_focusResource = 0;
    setMouseFocus(0, QPointF(), QPointF());
}

void Pointer::startGrab(PointerGrabber *grab)
{
    m_grab = grab;
    grab->m_pointer = this;

    if (m_current)
        grab->focus();
}

void Pointer::endGrab()
{
    m_grab = this;
    m_grab->focus();
}

void Pointer::setCurrent(QWaylandSurfaceView *surface, const QPointF &point)
{
    m_current = surface;
    m_currentPoint = point;
}

bool Pointer::buttonPressed() const
{
    return m_buttonCount > 0;
}

PointerGrabber *Pointer::currentGrab() const
{
    return m_grab;
}

Qt::MouseButton Pointer::grabButton() const
{
    return m_grabButton;
}

uint32_t Pointer::grabTime() const
{
    return m_grabTime;
}

uint32_t Pointer::grabSerial() const
{
    return m_grabSerial;
}

QWaylandSurfaceView *Pointer::focusSurface() const
{
    return m_focus;
}

QWaylandSurfaceView *Pointer::current() const
{
    return m_current;
}

QPointF Pointer::position() const
{
    return m_position;
}

QPointF Pointer::currentPosition() const
{
    return m_currentPoint;
}

QtWaylandServer::wl_pointer::Resource *Pointer::focusResource() const
{
    return m_focusResource;
}

void Pointer::pointer_destroy_resource(wl_pointer::Resource *resource)
{
    if (m_focusResource == resource)
        m_focusResource = 0;
}

void Pointer::setMouseFocus(QWaylandSurfaceView *surface, const QPointF &localPos, const QPointF &globalPos)
{
    m_position = globalPos;

    m_current = surface;
    m_currentPoint = localPos;

    m_grab->focus();
}

void Pointer::sendButton(uint32_t time, Qt::MouseButton button, uint32_t state)
{
    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());
    send_button(m_focusResource->handle, serial, time, toWaylandButton(button), state);
}

void Pointer::sendMousePressEvent(Qt::MouseButton button, const QPointF &localPos, const QPointF &globalPos)
{
    sendMouseMoveEvent(localPos, globalPos);
    uint32_t time = m_compositor->currentTimeMsecs();
    if (m_buttonCount == 0) {
        m_grabButton = button;
        m_grabTime = time;
    }
    m_buttonCount++;
    m_grab->button(time, button, WL_POINTER_BUTTON_STATE_PRESSED);

    if (m_buttonCount == 1)
        m_grabSerial = wl_display_get_serial(m_compositor->wl_display());
}

void Pointer::sendMouseReleaseEvent(Qt::MouseButton button, const QPointF &localPos, const QPointF &globalPos)
{
    sendMouseMoveEvent(localPos, globalPos);
    uint32_t time = m_compositor->currentTimeMsecs();
    m_buttonCount--;
    m_grab->button(time, button, WL_POINTER_BUTTON_STATE_RELEASED);

    if (m_buttonCount == 1)
        m_grabSerial = wl_display_get_serial(m_compositor->wl_display());
}

void Pointer::sendMouseMoveEvent(const QPointF &localPos, const QPointF &globalPos)
{
    uint32_t time = m_compositor->currentTimeMsecs();

    m_position = globalPos;
    m_currentPoint = localPos;

    m_grab->motion(time);
}

void Pointer::sendMouseWheelEvent(Qt::Orientation orientation, int delta)
{
    if (!m_focusResource)
        return;

    uint32_t time = m_compositor->currentTimeMsecs();
    uint32_t axis = orientation == Qt::Horizontal ? WL_POINTER_AXIS_HORIZONTAL_SCROLL
                                                  : WL_POINTER_AXIS_VERTICAL_SCROLL;
    send_axis(m_focusResource->handle, time, axis, wl_fixed_from_int(-delta / 12));
}

void Pointer::focus()
{
    if (buttonPressed())
        return;

    setFocus(m_current, m_currentPoint);
}

void Pointer::motion(uint32_t time)
{
    if (m_focusResource)
        send_motion(m_focusResource->handle, time,
                    wl_fixed_from_double(m_currentPoint.x()),
                    wl_fixed_from_double(m_currentPoint.y()));

}

void Pointer::button(uint32_t time, Qt::MouseButton button, uint32_t state)
{
    if (m_focusResource) {
        sendButton(time, button, state);
    }

    if (!buttonPressed() && state == WL_POINTER_BUTTON_STATE_RELEASED)
        setFocus(m_current, m_currentPoint);
}

void Pointer::pointer_set_cursor(wl_pointer::Resource *resource, uint32_t serial, wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y)
{
    Q_UNUSED(resource);
    Q_UNUSED(serial);

    if (!surface) {
        m_compositor->waylandCompositor()->setCursorSurface(NULL, 0, 0);
        return;
    }

    Surface *s = Surface::fromResource(surface);
    s->setCursorSurface(true);
    m_compositor->waylandCompositor()->setCursorSurface(s->waylandSurface(), hotspot_x, hotspot_y);
}

PointerGrabber::~PointerGrabber()
{
}

} // namespace QtWayland

QT_END_NAMESPACE
