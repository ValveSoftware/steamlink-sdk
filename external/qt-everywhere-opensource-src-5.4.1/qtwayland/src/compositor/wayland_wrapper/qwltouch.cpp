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

#include "qwltouch_p.h"

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"
#include "qwaylandsurfaceview.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

Touch::Touch(Compositor *compositor)
    : wl_touch()
    , m_compositor(compositor)
    , m_focus()
    , m_focusResource()
    , m_grab(this)
{
    m_grab->setTouch(this);
    connect(&m_focusDestroyListener, &WlListener::fired, this, &Touch::focusDestroyed);
}

void Touch::setFocus(QWaylandSurfaceView *surface)
{
    m_focusDestroyListener.reset();
    if (surface)
        m_focusDestroyListener.listenForDestruction(surface->surface()->handle()->resource()->handle);

    m_focus = surface;
    m_focusResource = surface ? resourceMap().value(surface->surface()->handle()->resource()->client()) : 0;
}

void Touch::startGrab(TouchGrabber *grab)
{
    m_grab = grab;
    grab->setTouch(this);
}

void Touch::endGrab()
{
    m_grab = this;
}

void Touch::focusDestroyed(void *data)
{
    Q_UNUSED(data)
    m_focusDestroyListener.reset();

    m_focus = 0;
    m_focusResource = 0;
}

void Touch::sendCancel()
{
    if (m_focusResource)
        send_cancel(m_focusResource->handle);
}

void Touch::sendFrame()
{
    if (m_focusResource)
        send_frame(m_focusResource->handle);
}

void Touch::sendDown(int touch_id, const QPointF &position)
{
    m_grab->down(m_compositor->currentTimeMsecs(), touch_id, position);
}

void Touch::sendMotion(int touch_id, const QPointF &position)
{
    m_grab->motion(m_compositor->currentTimeMsecs(), touch_id, position);
}

void Touch::sendUp(int touch_id)
{
    m_grab->up(m_compositor->currentTimeMsecs(), touch_id);
}

void Touch::down(uint32_t time, int touch_id, const QPointF &position)
{
    if (!m_focusResource || !m_focus)
        return;

    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());

    send_down(m_focusResource->handle, serial, time, m_focus->surface()->handle()->resource()->handle, touch_id,
              wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
}

void Touch::up(uint32_t time, int touch_id)
{
    if (!m_focusResource)
        return;

    uint32_t serial = wl_display_next_serial(m_compositor->wl_display());

    send_up(m_focusResource->handle, serial, time, touch_id);
}

void Touch::motion(uint32_t time, int touch_id, const QPointF &position)
{
    if (!m_focusResource)
        return;

    send_motion(m_focusResource->handle, time, touch_id,
                wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
}

TouchGrabber::TouchGrabber()
    : m_touch(0)
{
}

TouchGrabber::~TouchGrabber()
{
}

const Touch *TouchGrabber::touch() const
{
    return m_touch;
}

Touch *TouchGrabber::touch()
{
    return m_touch;
}

void TouchGrabber::setTouch(Touch *touch)
{
    m_touch = touch;
}

} // namespace QtWayland

QT_END_NAMESPACE
